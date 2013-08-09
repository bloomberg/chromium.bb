// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_connection.h"

#include <algorithm>

#include "base/logging.h"
#include "base/stl_util.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/quic_utils.h"

using base::hash_map;
using base::hash_set;
using base::StringPiece;
using std::list;
using std::make_pair;
using std::min;
using std::max;
using std::vector;
using std::set;
using std::string;

namespace net {
namespace {

// The largest gap in packets we'll accept without closing the connection.
// This will likely have to be tuned.
const QuicPacketSequenceNumber kMaxPacketGap = 5000;

// We want to make sure if we get a large nack packet, we don't queue up too
// many packets at once.  10 is arbitrary.
const int kMaxRetransmissionsPerAck = 10;

// TCP retransmits after 2 nacks.  We allow for a third in case of out-of-order
// delivery.
// TODO(ianswett): Change to match TCP's rule of retransmitting once an ack
// at least 3 sequence numbers larger arrives.
const size_t kNumberOfNacksBeforeRetransmission = 3;

// The maxiumum number of packets we'd like to queue.  We may end up queueing
// more in the case of many control frames.
// 6 is arbitrary.
const int kMaxPacketsToSerializeAtOnce = 6;

// Limit the number of packets we send per retransmission-alarm so we
// eventually cede.  10 is arbitrary.
const size_t kMaxPacketsPerRetransmissionAlarm = 10;

// Limit the number of FEC groups to two.  If we get enough out of order packets
// that this becomes limiting, we can revisit.
const size_t kMaxFecGroups = 2;

// Limit the number of undecryptable packets we buffer in
// expectation of the CHLO/SHLO arriving.
const size_t kMaxUndecryptablePackets = 10;

bool Near(QuicPacketSequenceNumber a, QuicPacketSequenceNumber b) {
  QuicPacketSequenceNumber delta = (a > b) ? a - b : b - a;
  return delta <= kMaxPacketGap;
}


// An alarm that is scheduled to send an ack if a timeout occurs.
class AckAlarm : public QuicAlarm::Delegate {
 public:
  explicit AckAlarm(QuicConnection* connection)
      : connection_(connection) {
  }

  virtual QuicTime OnAlarm() OVERRIDE {
    connection_->SendAck();
    return QuicTime::Zero();
  }

 private:
  QuicConnection* connection_;
};

// This alarm will be scheduled any time a data-bearing packet is sent out.
// When the alarm goes off, the connection checks to see if the oldest packets
// have been acked, and retransmit them if they have not.
class RetransmissionAlarm : public QuicAlarm::Delegate {
 public:
  explicit RetransmissionAlarm(QuicConnection* connection)
      : connection_(connection) {
  }

  virtual QuicTime OnAlarm() OVERRIDE {
    return connection_->OnRetransmissionTimeout();
  }

 private:
  QuicConnection* connection_;
};

// An alarm that is scheduled when the sent scheduler requires a
// a delay before sending packets and fires when the packet may be sent.
class SendAlarm : public QuicAlarm::Delegate {
 public:
  explicit SendAlarm(QuicConnection* connection)
      : connection_(connection) {
  }

  virtual QuicTime OnAlarm() OVERRIDE {
    connection_->OnCanWrite();
    // Never reschedule the alarm, since OnCanWrite does that.
    return QuicTime::Zero();
  }

 private:
  QuicConnection* connection_;
};

class TimeoutAlarm : public QuicAlarm::Delegate {
 public:
  explicit TimeoutAlarm(QuicConnection* connection)
      : connection_(connection) {
  }

  virtual QuicTime OnAlarm() OVERRIDE {
    connection_->CheckForTimeout();
    // Never reschedule the alarm, since CheckForTimeout does that.
    return QuicTime::Zero();
  }

 private:
  QuicConnection* connection_;
};

}  // namespace

#define ENDPOINT (is_server_ ? "Server: " : " Client: ")

QuicConnection::QuicConnection(QuicGuid guid,
                               IPEndPoint address,
                               QuicConnectionHelperInterface* helper,
                               bool is_server,
                               QuicVersion version)
    : framer_(version,
              helper->GetClock()->ApproximateNow(),
              is_server),
      helper_(helper),
      encryption_level_(ENCRYPTION_NONE),
      clock_(helper->GetClock()),
      random_generator_(helper->GetRandomGenerator()),
      guid_(guid),
      peer_address_(address),
      largest_seen_packet_with_ack_(0),
      handling_retransmission_timeout_(false),
      write_blocked_(false),
      ack_alarm_(helper->CreateAlarm(new AckAlarm(this))),
      retransmission_alarm_(helper->CreateAlarm(new RetransmissionAlarm(this))),
      send_alarm_(helper->CreateAlarm(new SendAlarm(this))),
      timeout_alarm_(helper->CreateAlarm(new TimeoutAlarm(this))),
      debug_visitor_(NULL),
      packet_creator_(guid_, &framer_, random_generator_, is_server),
      packet_generator_(this, NULL, &packet_creator_),
      idle_network_timeout_(
          QuicTime::Delta::FromSeconds(kDefaultInitialTimeoutSecs)),
      overall_connection_timeout_(QuicTime::Delta::Infinite()),
      creation_time_(clock_->ApproximateNow()),
      time_of_last_received_packet_(clock_->ApproximateNow()),
      time_of_last_sent_packet_(clock_->ApproximateNow()),
      congestion_manager_(clock_, kTCP),
      version_negotiation_state_(START_NEGOTIATION),
      max_packets_per_retransmission_alarm_(kMaxPacketsPerRetransmissionAlarm),
      is_server_(is_server),
      connected_(true),
      received_truncated_ack_(false),
      send_ack_in_response_to_packet_(false),
      address_migrating_(false) {
  helper_->SetConnection(this);
  timeout_alarm_->Set(clock_->ApproximateNow().Add(idle_network_timeout_));
  framer_.set_visitor(this);
  framer_.set_received_entropy_calculator(&received_packet_manager_);

  /*
  if (FLAGS_fake_packet_loss_percentage > 0) {
    int32 seed = RandomBase::WeakSeed32();
    LOG(INFO) << ENDPOINT << "Seeding packet loss with " << seed;
    random_.reset(new MTRandom(seed));
  }
  */
}

QuicConnection::~QuicConnection() {
  STLDeleteElements(&undecryptable_packets_);
  STLDeleteValues(&unacked_packets_);
  STLDeleteValues(&group_map_);
  for (QueuedPacketList::iterator it = queued_packets_.begin();
       it != queued_packets_.end(); ++it) {
    delete it->packet;
  }
}

bool QuicConnection::SelectMutualVersion(
    const QuicVersionVector& available_versions) {
  // Try to find the highest mutual version by iterating over supported
  // versions, starting with the highest, and breaking out of the loop once we
  // find a matching version in the provided available_versions vector.
  for (size_t i = 0; i < arraysize(kSupportedQuicVersions); ++i) {
    const QuicVersion& version = kSupportedQuicVersions[i];
    if (std::find(available_versions.begin(), available_versions.end(),
                  version) != available_versions.end()) {
      framer_.set_version(version);
      return true;
    }
  }

  return false;
}

void QuicConnection::OnError(QuicFramer* framer) {
  // Packets that we cannot decrypt are dropped.
  // TODO(rch): add stats to measure this.
  if (!connected_ || framer->error() == QUIC_DECRYPTION_FAILURE) {
    return;
  }
  SendConnectionClose(framer->error());
}

void QuicConnection::OnPacket() {
  DCHECK(last_stream_frames_.empty() &&
         last_goaway_frames_.empty() &&
         last_rst_frames_.empty() &&
         last_ack_frames_.empty() &&
         last_congestion_frames_.empty());
}

void QuicConnection::OnPublicResetPacket(
    const QuicPublicResetPacket& packet) {
  if (debug_visitor_) {
    debug_visitor_->OnPublicResetPacket(packet);
  }
  CloseConnection(QUIC_PUBLIC_RESET, true);
}

bool QuicConnection::OnProtocolVersionMismatch(QuicVersion received_version) {
  // TODO(satyamshekhar): Implement no server state in this mode.
  if (!is_server_) {
    LOG(DFATAL) << ENDPOINT << "Framer called OnProtocolVersionMismatch. "
                << "Closing connection.";
    CloseConnection(QUIC_INTERNAL_ERROR, false);
    return false;
  }
  DCHECK_NE(version(), received_version);

  if (debug_visitor_) {
    debug_visitor_->OnProtocolVersionMismatch(received_version);
  }

  switch (version_negotiation_state_) {
    case START_NEGOTIATION:
      if (!framer_.IsSupportedVersion(received_version)) {
        SendVersionNegotiationPacket();
        version_negotiation_state_ = NEGOTIATION_IN_PROGRESS;
        return false;
      }
      break;

    case NEGOTIATION_IN_PROGRESS:
      if (!framer_.IsSupportedVersion(received_version)) {
        // Drop packets which can't be parsed due to version mismatch.
        return false;
      }
      break;

    case NEGOTIATED_VERSION:
      // Might be old packets that were sent by the client before the version
      // was negotiated. Drop these.
      return false;

    default:
      DCHECK(false);
  }

  version_negotiation_state_ = NEGOTIATED_VERSION;

  // Store the new version.
  framer_.set_version(received_version);

  // TODO(satyamshekhar): Store the sequence number of this packet and close the
  // connection if we ever received a packet with incorrect version and whose
  // sequence number is greater.
  return true;
}

// Handles version negotiation for client connection.
void QuicConnection::OnVersionNegotiationPacket(
    const QuicVersionNegotiationPacket& packet) {
  if (is_server_) {
    LOG(DFATAL) << ENDPOINT << "Framer parsed VersionNegotiationPacket."
                << " Closing connection.";
    CloseConnection(QUIC_INTERNAL_ERROR, false);
    return;
  }
  if (debug_visitor_) {
    debug_visitor_->OnVersionNegotiationPacket(packet);
  }

  if (version_negotiation_state_ != START_NEGOTIATION) {
    // Possibly a duplicate version negotiation packet.
    return;
  }

  if (std::find(packet.versions.begin(),
                packet.versions.end(), version()) !=
      packet.versions.end()) {
    DLOG(WARNING) << ENDPOINT << "The server already supports our version. "
                  << "It should have accepted our connection.";
    // Just drop the connection.
    CloseConnection(QUIC_INVALID_VERSION_NEGOTIATION_PACKET, false);
    return;
  }

  if (!SelectMutualVersion(packet.versions)) {
    SendConnectionCloseWithDetails(QUIC_INVALID_VERSION,
                                   "no common version found");
    return;
  }

  version_negotiation_state_ = NEGOTIATION_IN_PROGRESS;
  RetransmitUnackedPackets(ALL_PACKETS);
}

void QuicConnection::OnRevivedPacket() {
}

bool QuicConnection::OnPacketHeader(const QuicPacketHeader& header) {
  if (debug_visitor_) {
    debug_visitor_->OnPacketHeader(header);
  }

  if (!ProcessValidatedPacket()) {
    return false;
  }

  // Will be decrement below if we fall through to return true;
  ++stats_.packets_dropped;

  if (header.public_header.guid != guid_) {
    DLOG(INFO) << ENDPOINT << "Ignoring packet from unexpected GUID: "
               << header.public_header.guid << " instead of " << guid_;
    return false;
  }

  if (!Near(header.packet_sequence_number,
            last_header_.packet_sequence_number)) {
    DLOG(INFO) << ENDPOINT << "Packet " << header.packet_sequence_number
               << " out of bounds.  Discarding";
    SendConnectionCloseWithDetails(QUIC_INVALID_PACKET_HEADER,
                                   "Packet sequence number out of bounds");
    return false;
  }

  // If this packet has already been seen, or that the sender
  // has told us will not be retransmitted, then stop processing the packet.
  if (!received_packet_manager_.IsAwaitingPacket(
          header.packet_sequence_number)) {
    return false;
  }

  if (version_negotiation_state_ != NEGOTIATED_VERSION) {
    if (is_server_) {
      if (!header.public_header.version_flag) {
        DLOG(WARNING) << ENDPOINT << "Got packet without version flag before "
                      << "version negotiated.";
        // Packets should have the version flag till version negotiation is
        // done.
        CloseConnection(QUIC_INVALID_VERSION, false);
        return false;
      } else {
        DCHECK_EQ(1u, header.public_header.versions.size());
        DCHECK_EQ(header.public_header.versions[0], version());
        version_negotiation_state_ = NEGOTIATED_VERSION;
      }
    } else {
      DCHECK(!header.public_header.version_flag);
      // If the client gets a packet without the version flag from the server
      // it should stop sending version since the version negotiation is done.
      packet_creator_.StopSendingVersion();
      version_negotiation_state_ = NEGOTIATED_VERSION;
    }
  }

  DCHECK_EQ(NEGOTIATED_VERSION, version_negotiation_state_);

  --stats_.packets_dropped;
  DVLOG(1) << ENDPOINT << "Received packet header: " << header;
  last_header_ = header;
  DCHECK(connected_);
  return true;
}

void QuicConnection::OnFecProtectedPayload(StringPiece payload) {
  DCHECK_EQ(IN_FEC_GROUP, last_header_.is_in_fec_group);
  DCHECK_NE(0u, last_header_.fec_group);
  QuicFecGroup* group = GetFecGroup();
  if (group != NULL) {
    group->Update(last_header_, payload);
  }
}

bool QuicConnection::OnStreamFrame(const QuicStreamFrame& frame) {
  DCHECK(connected_);
  if (debug_visitor_) {
    debug_visitor_->OnStreamFrame(frame);
  }
  last_stream_frames_.push_back(frame);
  return true;
}

bool QuicConnection::OnAckFrame(const QuicAckFrame& incoming_ack) {
  DCHECK(connected_);
  if (debug_visitor_) {
    debug_visitor_->OnAckFrame(incoming_ack);
  }
  DVLOG(1) << ENDPOINT << "OnAckFrame: " << incoming_ack;

  if (last_header_.packet_sequence_number <= largest_seen_packet_with_ack_) {
    DLOG(INFO) << ENDPOINT << "Received an old ack frame: ignoring";
    return true;
  }

  if (!ValidateAckFrame(incoming_ack)) {
    SendConnectionClose(QUIC_INVALID_ACK_DATA);
    return false;
  }
  last_ack_frames_.push_back(incoming_ack);
  return connected_;
}

void QuicConnection::ProcessAckFrame(const QuicAckFrame& incoming_ack) {
  largest_seen_packet_with_ack_ = last_header_.packet_sequence_number;

  received_truncated_ack_ =
      incoming_ack.received_info.missing_packets.size() >=
      QuicFramer::GetMaxUnackedPackets(last_header_);

  received_packet_manager_.UpdatePacketInformationReceivedByPeer(incoming_ack);
  received_packet_manager_.UpdatePacketInformationSentByPeer(incoming_ack);
  // Possibly close any FecGroups which are now irrelevant.
  CloseFecGroupsBefore(incoming_ack.sent_info.least_unacked + 1);

  sent_entropy_manager_.ClearEntropyBefore(
      received_packet_manager_.least_packet_awaited_by_peer() - 1);

  SequenceNumberSet acked_packets;
  HandleAckForSentPackets(incoming_ack, &acked_packets);
  HandleAckForSentFecPackets(incoming_ack, &acked_packets);
  if (acked_packets.size() > 0) {
    visitor_->OnAck(acked_packets);
  }
  congestion_manager_.OnIncomingAckFrame(incoming_ack,
                                         time_of_last_received_packet_);
}

bool QuicConnection::OnCongestionFeedbackFrame(
    const QuicCongestionFeedbackFrame& feedback) {
  DCHECK(connected_);
  if (debug_visitor_) {
    debug_visitor_->OnCongestionFeedbackFrame(feedback);
  }
  last_congestion_frames_.push_back(feedback);
  return connected_;
}

bool QuicConnection::ValidateAckFrame(const QuicAckFrame& incoming_ack) {
  if (incoming_ack.received_info.largest_observed >
      packet_creator_.sequence_number()) {
    DLOG(ERROR) << ENDPOINT << "Peer's observed unsent packet:"
                << incoming_ack.received_info.largest_observed << " vs "
                << packet_creator_.sequence_number();
    // We got an error for data we have not sent.  Error out.
    return false;
  }

  if (incoming_ack.received_info.largest_observed <
          received_packet_manager_.peer_largest_observed_packet()) {
    DLOG(ERROR) << ENDPOINT << "Peer's largest_observed packet decreased:"
                << incoming_ack.received_info.largest_observed << " vs "
                << received_packet_manager_.peer_largest_observed_packet();
    // A new ack has a diminished largest_observed value.  Error out.
    // If this was an old packet, we wouldn't even have checked.
    return false;
  }

  // We can't have too many unacked packets, or our ack frames go over
  // kMaxPacketSize.
  DCHECK_LE(incoming_ack.received_info.missing_packets.size(),
            QuicFramer::GetMaxUnackedPackets(last_header_));

  if (incoming_ack.sent_info.least_unacked <
      received_packet_manager_.peer_least_packet_awaiting_ack()) {
    DLOG(ERROR) << ENDPOINT << "Peer's sent low least_unacked: "
                << incoming_ack.sent_info.least_unacked << " vs "
                << received_packet_manager_.peer_least_packet_awaiting_ack();
    // We never process old ack frames, so this number should only increase.
    return false;
  }

  if (incoming_ack.sent_info.least_unacked >
      last_header_.packet_sequence_number) {
    DLOG(ERROR) << ENDPOINT << "Peer sent least_unacked:"
                << incoming_ack.sent_info.least_unacked
                << " greater than the enclosing packet sequence number:"
                << last_header_.packet_sequence_number;
    return false;
  }

  if (!incoming_ack.received_info.missing_packets.empty() &&
      *incoming_ack.received_info.missing_packets.rbegin() >
      incoming_ack.received_info.largest_observed) {
    DLOG(ERROR) << ENDPOINT << "Peer sent missing packet: "
                << *incoming_ack.received_info.missing_packets.rbegin()
                << " greater than largest observed: "
                << incoming_ack.received_info.largest_observed;
    return false;
  }

  if (!incoming_ack.received_info.missing_packets.empty() &&
      *incoming_ack.received_info.missing_packets.begin() <
      received_packet_manager_.least_packet_awaited_by_peer()) {
    DLOG(ERROR) << ENDPOINT << "Peer sent missing packet: "
                << *incoming_ack.received_info.missing_packets.begin()
                << "smaller than least_packet_awaited_by_peer_: "
                << received_packet_manager_.least_packet_awaited_by_peer();
    return false;
  }

  if (!sent_entropy_manager_.IsValidEntropy(
          incoming_ack.received_info.largest_observed,
          incoming_ack.received_info.missing_packets,
          incoming_ack.received_info.entropy_hash)) {
    DLOG(ERROR) << ENDPOINT << "Peer sent invalid entropy.";
    return false;
  }

  return true;
}

void QuicConnection::HandleAckForSentPackets(const QuicAckFrame& incoming_ack,
                                             SequenceNumberSet* acked_packets) {
  int retransmitted_packets = 0;
  // Go through the packets we have not received an ack for and see if this
  // incoming_ack shows they've been seen by the peer.
  UnackedPacketMap::iterator it = unacked_packets_.begin();
  while (it != unacked_packets_.end()) {
    QuicPacketSequenceNumber sequence_number = it->first;
    if (sequence_number >
        received_packet_manager_.peer_largest_observed_packet()) {
      // These are very new sequence_numbers.
      break;
    }
    RetransmittableFrames* unacked = it->second;
    if (!IsAwaitingPacket(incoming_ack.received_info, sequence_number)) {
      // Packet was acked, so remove it from our unacked packet list.
      DVLOG(1) << ENDPOINT <<"Got an ack for packet " << sequence_number;
      acked_packets->insert(sequence_number);
      delete unacked;
      unacked_packets_.erase(it++);
      retransmission_map_.erase(sequence_number);
    } else {
      // This is a packet which we planned on retransmitting and has not been
      // seen at the time of this ack being sent out.  See if it's our new
      // lowest unacked packet.
      DVLOG(1) << ENDPOINT << "still missing packet " << sequence_number;
      ++it;
      // The peer got packets after this sequence number.  This is an explicit
      // nack.
      RetransmissionMap::iterator retransmission_it =
          retransmission_map_.find(sequence_number);
      ++(retransmission_it->second.number_nacks);
      if (retransmission_it->second.number_nacks >=
             kNumberOfNacksBeforeRetransmission &&
          retransmitted_packets < kMaxRetransmissionsPerAck) {
        ++retransmitted_packets;
        DVLOG(1) << ENDPOINT << "Trying to retransmit packet "
                 << sequence_number
                 << " as it has been nacked 3 or more times.";
        // RetransmitPacket will retransmit with a new sequence_number.
        RetransmitPacket(sequence_number);
      }
    }
  }
}

void QuicConnection::HandleAckForSentFecPackets(
    const QuicAckFrame& incoming_ack, SequenceNumberSet* acked_packets) {
  UnackedPacketMap::iterator it = unacked_fec_packets_.begin();
  while (it != unacked_fec_packets_.end()) {
    QuicPacketSequenceNumber sequence_number = it->first;
    if (sequence_number >
        received_packet_manager_.peer_largest_observed_packet()) {
      break;
    }
    if (!IsAwaitingPacket(incoming_ack.received_info, sequence_number)) {
      DVLOG(1) << ENDPOINT << "Got an ack for fec packet: " << sequence_number;
      acked_packets->insert(sequence_number);
      unacked_fec_packets_.erase(it++);
    } else {
      DVLOG(1) << ENDPOINT << "Still missing ack for fec packet: "
               << sequence_number;
      ++it;
    }
  }
}

void QuicConnection::OnFecData(const QuicFecData& fec) {
  DCHECK_EQ(IN_FEC_GROUP, last_header_.is_in_fec_group);
  DCHECK_NE(0u, last_header_.fec_group);
  QuicFecGroup* group = GetFecGroup();
  if (group != NULL) {
    group->UpdateFec(last_header_.packet_sequence_number,
                     last_header_.entropy_flag, fec);
  }
}

bool QuicConnection::OnRstStreamFrame(const QuicRstStreamFrame& frame) {
  DCHECK(connected_);
  if (debug_visitor_) {
    debug_visitor_->OnRstStreamFrame(frame);
  }
  DLOG(INFO) << ENDPOINT << "Stream reset with error "
             << QuicUtils::StreamErrorToString(frame.error_code);
  last_rst_frames_.push_back(frame);
  return connected_;
}

bool QuicConnection::OnConnectionCloseFrame(
    const QuicConnectionCloseFrame& frame) {
  DCHECK(connected_);
  if (debug_visitor_) {
    debug_visitor_->OnConnectionCloseFrame(frame);
  }
  DLOG(INFO) << ENDPOINT << "Connection closed with error "
             << QuicUtils::ErrorToString(frame.error_code)
             << " " << frame.error_details;
  CloseConnection(frame.error_code, true);
  DCHECK(!connected_);
  return false;
}

bool QuicConnection::OnGoAwayFrame(const QuicGoAwayFrame& frame) {
  DCHECK(connected_);
  DLOG(INFO) << ENDPOINT << "Go away received with error "
             << QuicUtils::ErrorToString(frame.error_code)
             << " and reason:" << frame.reason_phrase;
  last_goaway_frames_.push_back(frame);
  return connected_;
}

void QuicConnection::OnPacketComplete() {
  // Don't do anything if this packet closed the connection.
  if (!connected_) {
    ClearLastFrames();
    return;
  }

  DLOG(INFO) << ENDPOINT << (last_packet_revived_ ? "Revived" : "Got")
             << " packet " << last_header_.packet_sequence_number
             << " with " << last_ack_frames_.size() << " acks, "
             << last_congestion_frames_.size() << " congestions, "
             << last_goaway_frames_.size() << " goaways, "
             << last_rst_frames_.size() << " rsts, "
             << last_stream_frames_.size()
             << " stream frames for " << last_header_.public_header.guid;
  if (!last_packet_revived_) {
    congestion_manager_.RecordIncomingPacket(
        last_size_, last_header_.packet_sequence_number,
        time_of_last_received_packet_, last_packet_revived_);
  }

  // Must called before ack processing, because processing acks removes entries
  // from unacket_packets_, increasing the least_unacked.
  const bool last_packet_should_instigate_ack = ShouldLastPacketInstigateAck();

  if ((last_stream_frames_.empty() ||
       visitor_->OnPacket(self_address_, peer_address_,
                          last_header_, last_stream_frames_))) {
    received_packet_manager_.RecordPacketReceived(
        last_header_, time_of_last_received_packet_);
  }

  // Process stream resets, then acks, then congestion feedback.
  for (size_t i = 0; i < last_goaway_frames_.size(); ++i) {
    visitor_->OnGoAway(last_goaway_frames_[i]);
  }
  for (size_t i = 0; i < last_rst_frames_.size(); ++i) {
    visitor_->OnRstStream(last_rst_frames_[i]);
  }
  for (size_t i = 0; i < last_ack_frames_.size(); ++i) {
    ProcessAckFrame(last_ack_frames_[i]);
  }
  for (size_t i = 0; i < last_congestion_frames_.size(); ++i) {
    congestion_manager_.OnIncomingQuicCongestionFeedbackFrame(
        last_congestion_frames_[i], time_of_last_received_packet_);
  }

  MaybeSendInResponseToPacket(last_packet_should_instigate_ack);

  ClearLastFrames();
}

void QuicConnection::ClearLastFrames() {
  last_stream_frames_.clear();
  last_goaway_frames_.clear();
  last_rst_frames_.clear();
  last_ack_frames_.clear();
  last_congestion_frames_.clear();
}

QuicAckFrame* QuicConnection::CreateAckFrame() {
  QuicAckFrame* outgoing_ack = new QuicAckFrame();
  received_packet_manager_.UpdateReceivedPacketInfo(
      &(outgoing_ack->received_info), clock_->ApproximateNow());
  UpdateSentPacketInfo(&(outgoing_ack->sent_info));
  DVLOG(1) << ENDPOINT << "Creating ack frame: " << *outgoing_ack;
  return outgoing_ack;
}

QuicCongestionFeedbackFrame* QuicConnection::CreateFeedbackFrame() {
  return new QuicCongestionFeedbackFrame(outgoing_congestion_feedback_);
}

bool QuicConnection::ShouldLastPacketInstigateAck() {
  if (!last_stream_frames_.empty() ||
      !last_goaway_frames_.empty() ||
      !last_rst_frames_.empty()) {
    return true;
  }

  // If the peer is still waiting for a packet that we are no
  // longer planning to send, we should send an ack to raise
  // the high water mark.
  if (!last_ack_frames_.empty() &&
      !last_ack_frames_.back().received_info.missing_packets.empty() &&
      !unacked_packets_.empty()) {
    if (unacked_packets_.begin()->first >
        *last_ack_frames_.back().received_info.missing_packets.begin()) {
      return true;
    }
  }

  return false;
}

void QuicConnection::MaybeSendInResponseToPacket(
    bool last_packet_should_instigate_ack) {
  // TODO(ianswett): Better merge these two blocks to queue up an ack if
  // necessary, then either only send the ack or bundle it with other data.
  if (!last_ack_frames_.empty()) {
    // Now the we have received an ack, we might be able to send packets which
    // are queued locally, or drain streams which are blocked.
    QuicTime::Delta delay = congestion_manager_.TimeUntilSend(
        time_of_last_received_packet_, NOT_RETRANSMISSION,
        HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE);
    if (delay.IsZero()) {
      send_alarm_->Cancel();
      WriteIfNotBlocked();
    } else if (!delay.IsInfinite()) {
      send_alarm_->Cancel();
      send_alarm_->Set(time_of_last_received_packet_.Add(delay));
    }
  }

  if (!last_packet_should_instigate_ack) {
    return;
  }

  if (send_ack_in_response_to_packet_) {
    SendAck();
  } else if (!last_stream_frames_.empty()) {
    // TODO(alyssar) this case should really be "if the packet contained any
    // non-ack frame", rather than "if the packet contained a stream frame"
    if (!ack_alarm_->IsSet()) {
      ack_alarm_->Set(clock_->ApproximateNow().Add(
          congestion_manager_.DefaultRetransmissionTime()));
    }
  }
  send_ack_in_response_to_packet_ = !send_ack_in_response_to_packet_;
}

void QuicConnection::SendVersionNegotiationPacket() {
  QuicVersionVector supported_versions;
  for (size_t i = 0; i < arraysize(kSupportedQuicVersions); ++i) {
    supported_versions.push_back(kSupportedQuicVersions[i]);
  }
  QuicEncryptedPacket* encrypted =
      packet_creator_.SerializeVersionNegotiationPacket(supported_versions);
  // TODO(satyamshekhar): implement zero server state negotiation.
  int error;
  helper_->WritePacketToWire(*encrypted, &error);
  delete encrypted;
}

QuicConsumedData QuicConnection::SendStreamData(QuicStreamId id,
                                                StringPiece data,
                                                QuicStreamOffset offset,
                                                bool fin) {
  return packet_generator_.ConsumeData(id, data, offset, fin);
}

void QuicConnection::SendRstStream(QuicStreamId id,
                                   QuicRstStreamErrorCode error) {
  packet_generator_.AddControlFrame(
      QuicFrame(new QuicRstStreamFrame(id, error)));
}

const QuicConnectionStats& QuicConnection::GetStats() {
  // Update rtt and estimated bandwidth.
  stats_.rtt = congestion_manager_.SmoothedRtt().ToMicroseconds();
  stats_.estimated_bandwidth =
      congestion_manager_.BandwidthEstimate().ToBytesPerSecond();
  return stats_;
}

void QuicConnection::ProcessUdpPacket(const IPEndPoint& self_address,
                                      const IPEndPoint& peer_address,
                                      const QuicEncryptedPacket& packet) {
  if (!connected_) {
    return;
  }
  if (debug_visitor_) {
    debug_visitor_->OnPacketReceived(self_address, peer_address, packet);
  }
  last_packet_revived_ = false;
  last_size_ = packet.length();

  address_migrating_ = false;

  if (peer_address_.address().empty()) {
    peer_address_ = peer_address;
  }
  if (self_address_.address().empty()) {
    self_address_ = self_address;
  }

  if (!(peer_address == peer_address_ && self_address == self_address_)) {
    address_migrating_ = true;
  }

  stats_.bytes_received += packet.length();
  ++stats_.packets_received;

  if (!framer_.ProcessPacket(packet)) {
    // If we are unable to decrypt this packet, it might be
    // because the CHLO or SHLO packet was lost.
    if (encryption_level_ != ENCRYPTION_FORWARD_SECURE &&
        framer_.error() == QUIC_DECRYPTION_FAILURE &&
        undecryptable_packets_.size() < kMaxUndecryptablePackets) {
      QueueUndecryptablePacket(packet);
    }
    DVLOG(1) << ENDPOINT << "Unable to process packet.  Last packet processed: "
             << last_header_.packet_sequence_number;
    return;
  }
  MaybeProcessUndecryptablePackets();
  MaybeProcessRevivedPacket();
}

bool QuicConnection::OnCanWrite() {
  write_blocked_ = false;
  return DoWrite();
}

bool QuicConnection::WriteIfNotBlocked() {
  if (write_blocked_) {
    return false;
  }
  return DoWrite();
}

bool QuicConnection::DoWrite() {
  DCHECK(!write_blocked_);
  WriteQueuedPackets();

  // Sending queued packets may have caused the socket to become write blocked,
  // or the congestion manager to prohibit sending.  If we've sent everything
  // we had queued and we're still not blocked, let the visitor know it can
  // write more.
  if (CanWrite(NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA,
               NOT_HANDSHAKE)) {
    packet_generator_.StartBatchOperations();
    bool all_bytes_written = visitor_->OnCanWrite();
    packet_generator_.FinishBatchOperations();

    // After the visitor writes, it may have caused the socket to become write
    // blocked or the congestion manager to prohibit sending, so check again.
    if (!write_blocked_ && !all_bytes_written &&
        CanWrite(NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA,
                 NOT_HANDSHAKE)) {
      // We're not write blocked, but some stream didn't write out all of its
      // bytes.  Register for 'immediate' resumption so we'll keep writing after
      // other quic connections have had a chance to use the socket.
      send_alarm_->Cancel();
      send_alarm_->Set(clock_->ApproximateNow());
    }
  }

  return !write_blocked_;
}

bool QuicConnection::ProcessValidatedPacket() {
  if (address_migrating_) {
    SendConnectionCloseWithDetails(
        QUIC_ERROR_MIGRATING_ADDRESS,
        "Address migration is not yet a supported feature");
    return false;
  }
  time_of_last_received_packet_ = clock_->Now();
  DVLOG(1) << ENDPOINT << "time of last received packet: "
           << time_of_last_received_packet_.ToDebuggingValue();
  return true;
}

bool QuicConnection::WriteQueuedPackets() {
  DCHECK(!write_blocked_);

  size_t num_queued_packets = queued_packets_.size() + 1;
  QueuedPacketList::iterator packet_iterator = queued_packets_.begin();
  while (!write_blocked_ && packet_iterator != queued_packets_.end()) {
    // Ensure that from one iteration of this loop to the next we
    // succeeded in sending a packet so we don't infinitely loop.
    // TODO(rch): clean up and close the connection if we really hit this.
    DCHECK_LT(queued_packets_.size(), num_queued_packets);
    num_queued_packets = queued_packets_.size();
    if (WritePacket(packet_iterator->encryption_level,
                    packet_iterator->sequence_number,
                    packet_iterator->packet,
                    packet_iterator->retransmittable,
                    NO_FORCE)) {
      packet_iterator = queued_packets_.erase(packet_iterator);
    } else {
      // Continue, because some queued packets may still be writable.
      // This can happen if a retransmit send fail.
      ++packet_iterator;
    }
  }

  return !write_blocked_;
}

bool QuicConnection::MaybeRetransmitPacketForRTO(
    QuicPacketSequenceNumber sequence_number) {
  DCHECK_EQ(ContainsKey(unacked_packets_, sequence_number),
            ContainsKey(retransmission_map_, sequence_number));

  if (!ContainsKey(unacked_packets_, sequence_number)) {
    DVLOG(2) << ENDPOINT << "alarm fired for " << sequence_number
             << " but it has been acked or already retransmitted with"
             << " different sequence number.";
    // So no extra delay is added for this packet.
    return true;
  }

  RetransmissionMap::iterator retransmission_it =
      retransmission_map_.find(sequence_number);
  // If the packet hasn't been acked and we're getting truncated acks, ignore
  // any RTO for packets larger than the peer's largest observed packet; it may
  // have been received by the peer and just wasn't acked due to the ack frame
  // running out of space.
  if (received_truncated_ack_ && sequence_number >
      received_packet_manager_.peer_largest_observed_packet() &&
      // We allow retransmission of already retransmitted packets so that we
      // retransmit packets that were retransmissions of the packet with
      // sequence number < the largest observed field of the truncated ack.
      retransmission_it->second.number_retransmissions == 0) {
    return false;
  } else {
    ++stats_.rto_count;
    RetransmitPacket(sequence_number);
    return true;
  }
}

void QuicConnection::RetransmitUnackedPackets(
    RetransmissionType retransmission_type) {
  if (unacked_packets_.empty()) {
    return;
  }
  UnackedPacketMap::iterator next_it = unacked_packets_.begin();
  QuicPacketSequenceNumber end_sequence_number =
      unacked_packets_.rbegin()->first;
  do {
    UnackedPacketMap::iterator current_it = next_it;
    ++next_it;

    if (retransmission_type == ALL_PACKETS ||
        current_it->second->encryption_level() == ENCRYPTION_INITIAL) {
      // TODO(satyamshekhar): Think about congestion control here.
      // Specifically, about the retransmission count of packets being sent
      // proactively to achieve 0 (minimal) RTT.
      RetransmitPacket(current_it->first);
    }
  } while (next_it != unacked_packets_.end() &&
           next_it->first <= end_sequence_number);
}

void QuicConnection::RetransmitPacket(
    QuicPacketSequenceNumber sequence_number) {
  UnackedPacketMap::iterator unacked_it =
      unacked_packets_.find(sequence_number);
  RetransmissionMap::iterator retransmission_it =
      retransmission_map_.find(sequence_number);
  // There should always be an entry corresponding to |sequence_number| in
  // both |retransmission_map_| and |unacked_packets_|. Retransmissions due to
  // RTO for sequence numbers that are already acked or retransmitted are
  // ignored by MaybeRetransmitPacketForRTO.
  DCHECK(unacked_it != unacked_packets_.end());
  DCHECK(retransmission_it != retransmission_map_.end());
  RetransmittableFrames* unacked = unacked_it->second;
  // TODO(pwestin): Need to fix potential issue with FEC and a 1 packet
  // congestion window see b/8331807 for details.
  congestion_manager_.AbandoningPacket(sequence_number);

  // Re-packetize the frames with a new sequence number for retransmission.
  // Retransmitted data packets do not use FEC, even when it's enabled.
  SerializedPacket serialized_packet =
      packet_creator_.SerializeAllFrames(unacked->frames());
  RetransmissionInfo retransmission_info(serialized_packet.sequence_number);
  retransmission_info.number_retransmissions =
      retransmission_it->second.number_retransmissions + 1;
  // Remove info with old sequence number.
  unacked_packets_.erase(unacked_it);
  retransmission_map_.erase(retransmission_it);
  DVLOG(1) << ENDPOINT << "Retransmitting unacked packet " << sequence_number
           << " as " << serialized_packet.sequence_number;
  DCHECK(unacked_packets_.empty() ||
         unacked_packets_.rbegin()->first < serialized_packet.sequence_number);
  unacked_packets_.insert(make_pair(serialized_packet.sequence_number,
                                    unacked));
  retransmission_map_.insert(make_pair(serialized_packet.sequence_number,
                                       retransmission_info));
  SendOrQueuePacket(unacked->encryption_level(),
                    serialized_packet.sequence_number,
                    serialized_packet.packet,
                    serialized_packet.entropy_hash,
                    HAS_RETRANSMITTABLE_DATA);
}

bool QuicConnection::CanWrite(Retransmission retransmission,
                              HasRetransmittableData retransmittable,
                              IsHandshake handshake) {
  // TODO(ianswett): If the packet is a retransmit, the current send alarm may
  // be too long.
  if (write_blocked_ || send_alarm_->IsSet()) {
    return false;
  }

  QuicTime now = clock_->Now();
  QuicTime::Delta delay = congestion_manager_.TimeUntilSend(
      now, retransmission, retransmittable, handshake);
  if (delay.IsInfinite()) {
    return false;
  }

  // If the scheduler requires a delay, then we can not send this packet now.
  if (!delay.IsZero()) {
    send_alarm_->Cancel();
    send_alarm_->Set(now.Add(delay));
    return false;
  }
  return true;
}

bool QuicConnection::IsRetransmission(
    QuicPacketSequenceNumber sequence_number) {
  RetransmissionMap::iterator it = retransmission_map_.find(sequence_number);
  return it != retransmission_map_.end() &&
      it->second.number_retransmissions > 0;
}

void QuicConnection::SetupRetransmission(
    QuicPacketSequenceNumber sequence_number,
    EncryptionLevel level) {
  RetransmissionMap::iterator it = retransmission_map_.find(sequence_number);
  if (it == retransmission_map_.end()) {
    DVLOG(1) << ENDPOINT << "Will not retransmit packet " << sequence_number;
    return;
  }

  RetransmissionInfo retransmission_info = it->second;
  // TODO(rch): consider using a much smaller retransmisison_delay
  // for the ENCRYPTION_NONE packets.
  size_t effective_retransmission_count =
      level == ENCRYPTION_NONE ? 0 : retransmission_info.number_retransmissions;
  QuicTime::Delta retransmission_delay =
      congestion_manager_.GetRetransmissionDelay(
          unacked_packets_.size(),
          effective_retransmission_count);

  retransmission_timeouts_.push(RetransmissionTime(
      sequence_number,
      clock_->ApproximateNow().Add(retransmission_delay),
      false));

  // Do not set the retransmisson alarm if we're already handling the
  // retransmission alarm because the retransmission alarm will be reset when
  // OnRetransmissionTimeout completes.
  if (!handling_retransmission_timeout_ && !retransmission_alarm_->IsSet()) {
    retransmission_alarm_->Set(
        clock_->ApproximateNow().Add(retransmission_delay));
  }
  // TODO(satyamshekhar): restore packet reordering with Ian's TODO in
  // SendStreamData().
}

void QuicConnection::SetupAbandonFecTimer(
    QuicPacketSequenceNumber sequence_number) {
  DCHECK(ContainsKey(unacked_fec_packets_, sequence_number));
  QuicTime::Delta retransmission_delay =
      QuicTime::Delta::FromMilliseconds(
          congestion_manager_.DefaultRetransmissionTime().ToMilliseconds() * 3);
  retransmission_timeouts_.push(RetransmissionTime(
      sequence_number,
      clock_->ApproximateNow().Add(retransmission_delay),
      true));
}

void QuicConnection::DropPacket(QuicPacketSequenceNumber sequence_number) {
  UnackedPacketMap::iterator unacked_it =
      unacked_packets_.find(sequence_number);
  // Packet was not meant to be retransmitted.
  if (unacked_it == unacked_packets_.end()) {
    DCHECK(!ContainsKey(retransmission_map_, sequence_number));
    return;
  }
  // Delete the unacked packet.
  delete unacked_it->second;
  unacked_packets_.erase(unacked_it);
  retransmission_map_.erase(sequence_number);
  return;
}

bool QuicConnection::WritePacket(EncryptionLevel level,
                                 QuicPacketSequenceNumber sequence_number,
                                 QuicPacket* packet,
                                 HasRetransmittableData retransmittable,
                                 Force forced) {
  if (!connected_) {
    DLOG(INFO) << ENDPOINT
               << "Not sending packet as connection is disconnected.";
    delete packet;
    // Returning true because we deleted the packet and the caller shouldn't
    // delete it again.
    return true;
  }

  if (encryption_level_ == ENCRYPTION_FORWARD_SECURE &&
      level == ENCRYPTION_NONE) {
    // Drop packets that are NULL encrypted since the peer won't accept them
    // anymore.
    DLOG(INFO) << ENDPOINT << "Dropped packet: " << sequence_number
               << " since the packet is NULL encrypted.";
    DropPacket(sequence_number);
    delete packet;
    return true;
  }

  Retransmission retransmission = IsRetransmission(sequence_number) ?
      IS_RETRANSMISSION : NOT_RETRANSMISSION;
  IsHandshake handshake = level == ENCRYPTION_NONE ? IS_HANDSHAKE
                                                   : NOT_HANDSHAKE;

  // If we are not forced and we can't write, then simply return false;
  if (forced == NO_FORCE &&
      !CanWrite(retransmission, retransmittable, handshake)) {
    return false;
  }

  scoped_ptr<QuicEncryptedPacket> encrypted(
      framer_.EncryptPacket(level, sequence_number, *packet));
  DLOG(INFO) << ENDPOINT << "Sending packet number " << sequence_number
             << " : " << (packet->is_fec_packet() ? "FEC " :
                 (retransmittable == HAS_RETRANSMITTABLE_DATA
                      ? "data bearing " : " ack only "))
             << ", encryption level: "
             << QuicUtils::EncryptionLevelToString(level)
             << ", length:" << packet->length();
  DVLOG(2) << ENDPOINT << "packet(" << sequence_number << "): " << std::endl
           << QuicUtils::StringToHexASCIIDump(packet->AsStringPiece());

  DCHECK(encrypted->length() <= kMaxPacketSize)
      << "Packet " << sequence_number << " will not be read; too large: "
      << packet->length() << " " << encrypted->length() << " "
      << " forced: " << (forced == FORCE ? "yes" : "no");

  int error;
  QuicTime now = clock_->Now();
  if (!retransmission) {
    time_of_last_sent_packet_ = now;
  }
  DVLOG(1) << ENDPOINT << "time of last sent packet: "
           << now.ToDebuggingValue();
  if (WritePacketToWire(sequence_number, level, *encrypted, &error) == -1) {
    if (helper_->IsWriteBlocked(error)) {
      // TODO(satyashekhar): It might be more efficient (fewer system calls), if
      // all connections share this variable i.e this becomes a part of
      // PacketWriterInterface.
      write_blocked_ = true;
      // If the socket buffers the the data, then the packet should not
      // be queued and sent again, which would result in an unnecessary
      // duplicate packet being sent.
      return helper_->IsWriteBlockedDataBuffered();
    }
    // We can't send an error as the socket is presumably borked.
    CloseConnection(QUIC_PACKET_WRITE_ERROR, false);
    return false;
  }

  // Set the retransmit alarm only when we have sent the packet to the client
  // and not when it goes to the pending queue, otherwise we will end up adding
  // an entry to retransmission_timeout_ every time we attempt a write.
  if (retransmittable == HAS_RETRANSMITTABLE_DATA) {
    SetupRetransmission(sequence_number, level);
  } else if (packet->is_fec_packet()) {
    SetupAbandonFecTimer(sequence_number);
  }

  congestion_manager_.SentPacket(sequence_number, now, packet->length(),
                                 retransmission);

  stats_.bytes_sent += encrypted->length();
  ++stats_.packets_sent;

  if (retransmission == IS_RETRANSMISSION) {
    stats_.bytes_retransmitted += encrypted->length();
    ++stats_.packets_retransmitted;
  }

  delete packet;
  return true;
}

int QuicConnection::WritePacketToWire(QuicPacketSequenceNumber sequence_number,
                                      EncryptionLevel level,
                                      const QuicEncryptedPacket& packet,
                                      int* error) {
  int bytes_written = helper_->WritePacketToWire(packet, error);
  if (debug_visitor_) {
    // WritePacketToWire returned -1, then |error| will be populated with
    // an error code, which we want to pass along to the visitor.
    debug_visitor_->OnPacketSent(sequence_number, level, packet,
                                 bytes_written == -1 ? *error : bytes_written);
  }
  return bytes_written;
}

bool QuicConnection::OnSerializedPacket(
    const SerializedPacket& serialized_packet) {
  if (serialized_packet.retransmittable_frames != NULL) {
    DCHECK(unacked_packets_.empty() ||
           unacked_packets_.rbegin()->first <
               serialized_packet.sequence_number);
    // Retransmitted frames will be sent with the same encryption level as the
    // original.
    serialized_packet.retransmittable_frames->set_encryption_level(
        encryption_level_);
    unacked_packets_.insert(
        make_pair(serialized_packet.sequence_number,
                  serialized_packet.retransmittable_frames));
    // All unacked packets might be retransmitted.
    retransmission_map_.insert(
        make_pair(serialized_packet.sequence_number,
                  RetransmissionInfo(serialized_packet.sequence_number)));
  } else if (serialized_packet.packet->is_fec_packet()) {
    unacked_fec_packets_.insert(make_pair(
        serialized_packet.sequence_number,
        serialized_packet.retransmittable_frames));
  }
  return SendOrQueuePacket(encryption_level_,
                           serialized_packet.sequence_number,
                           serialized_packet.packet,
                           serialized_packet.entropy_hash,
                           serialized_packet.retransmittable_frames != NULL ?
                               HAS_RETRANSMITTABLE_DATA :
                               NO_RETRANSMITTABLE_DATA);
}

bool QuicConnection::SendOrQueuePacket(EncryptionLevel level,
                                       QuicPacketSequenceNumber sequence_number,
                                       QuicPacket* packet,
                                       QuicPacketEntropyHash entropy_hash,
                                       HasRetransmittableData retransmittable) {
  sent_entropy_manager_.RecordPacketEntropyHash(sequence_number, entropy_hash);
  if (!WritePacket(level, sequence_number, packet, retransmittable, NO_FORCE)) {
    queued_packets_.push_back(QueuedPacket(sequence_number, packet, level,
                                           retransmittable));
    return false;
  }
  return true;
}

bool QuicConnection::ShouldSimulateLostPacket() {
  // TODO(rch): enable this
  return false;
  /*
  return FLAGS_fake_packet_loss_percentage > 0 &&
      random_->Rand32() % 100 < FLAGS_fake_packet_loss_percentage;
  */
}

void QuicConnection::UpdateSentPacketInfo(SentPacketInfo* sent_info) {
  if (!unacked_packets_.empty()) {
    sent_info->least_unacked = unacked_packets_.begin()->first;
  } else {
    // If there are no unacked packets, set the least unacked packet to
    // sequence_number() + 1 since that will be the sequence number of this
    // ack packet whenever it is sent.
    sent_info->least_unacked = packet_creator_.sequence_number() + 1;
  }
  sent_info->entropy_hash = sent_entropy_manager_.EntropyHash(
      sent_info->least_unacked - 1);
}

void QuicConnection::SendAck() {
  ack_alarm_->Cancel();

  // TODO(rch): delay this until the CreateFeedbackFrame
  // method is invoked.  This requires changes SetShouldSendAck
  // to be a no-arg method, and re-jiggering its implementation.
  bool send_feedback = false;
  if (congestion_manager_.GenerateCongestionFeedback(
          &outgoing_congestion_feedback_)) {
    DVLOG(1) << ENDPOINT << "Sending feedback "
             << outgoing_congestion_feedback_;
    send_feedback = true;
  }

  packet_generator_.SetShouldSendAck(send_feedback);
}

void QuicConnection::MaybeAbandonFecPacket(
    QuicPacketSequenceNumber sequence_number) {
  if (!ContainsKey(unacked_fec_packets_, sequence_number)) {
    DVLOG(2) << ENDPOINT << "no need to abandon fec packet: "
             << sequence_number << "; it's already acked'";
    return;
  }
  congestion_manager_.AbandoningPacket(sequence_number);
  // TODO(satyashekhar): Should this decrease the congestion window?
}

QuicTime QuicConnection::OnRetransmissionTimeout() {
  // This guards against registering the alarm later than we should.
  //
  // If we have packet A and B in the list and we call
  // MaybeRetransmitPacketForRTO on A, that may trigger a call to
  // SetRetransmissionAlarm if A is retransmitted as C.  In that case we
  // don't want to register the alarm under SetRetransmissionAlarm; we
  // want to set it to the RTO of B when we return from this function.
  handling_retransmission_timeout_ = true;

  for (size_t i = 0; i < max_packets_per_retransmission_alarm_ &&
           !retransmission_timeouts_.empty(); ++i) {
    RetransmissionTime retransmission_time = retransmission_timeouts_.top();
    DCHECK(retransmission_time.scheduled_time.IsInitialized());
    if (retransmission_time.scheduled_time > clock_->ApproximateNow()) {
      break;
    }
    retransmission_timeouts_.pop();

    if (retransmission_time.for_fec) {
      MaybeAbandonFecPacket(retransmission_time.sequence_number);
      continue;
    } else if (
        !MaybeRetransmitPacketForRTO(retransmission_time.sequence_number)) {
      DLOG(INFO) << ENDPOINT << "MaybeRetransmitPacketForRTO failed: "
                 << "adding an extra delay for "
                 << retransmission_time.sequence_number;
      retransmission_time.scheduled_time = clock_->ApproximateNow().Add(
          congestion_manager_.DefaultRetransmissionTime());
      retransmission_timeouts_.push(retransmission_time);
    }
  }

  handling_retransmission_timeout_ = false;

  if (retransmission_timeouts_.empty()) {
    return QuicTime::Zero();
  }

  // We have packets remaining.  Return the absolute RTO of the oldest packet
  // on the list.
  return retransmission_timeouts_.top().scheduled_time;
}

void QuicConnection::SetEncrypter(EncryptionLevel level,
                                  QuicEncrypter* encrypter) {
  framer_.SetEncrypter(level, encrypter);
}

const QuicEncrypter* QuicConnection::encrypter(EncryptionLevel level) const {
  return framer_.encrypter(level);
}

void QuicConnection::SetDefaultEncryptionLevel(
    EncryptionLevel level) {
  encryption_level_ = level;
}

void QuicConnection::SetDecrypter(QuicDecrypter* decrypter) {
  framer_.SetDecrypter(decrypter);
}

void QuicConnection::SetAlternativeDecrypter(QuicDecrypter* decrypter,
                                             bool latch_once_used) {
  framer_.SetAlternativeDecrypter(decrypter, latch_once_used);
}

const QuicDecrypter* QuicConnection::decrypter() const {
  return framer_.decrypter();
}

const QuicDecrypter* QuicConnection::alternative_decrypter() const {
  return framer_.alternative_decrypter();
}

void QuicConnection::QueueUndecryptablePacket(
    const QuicEncryptedPacket& packet) {
  DVLOG(1) << ENDPOINT << "Queueing undecryptable packet.";
  char* data = new char[packet.length()];
  memcpy(data, packet.data(), packet.length());
  undecryptable_packets_.push_back(
      new QuicEncryptedPacket(data, packet.length(), true));
}

void QuicConnection::MaybeProcessUndecryptablePackets() {
  if (undecryptable_packets_.empty() ||
      encryption_level_ == ENCRYPTION_NONE) {
    return;
  }

  while (connected_ && !undecryptable_packets_.empty()) {
    DVLOG(1) << ENDPOINT << "Attempting to process undecryptable packet";
    QuicEncryptedPacket* packet = undecryptable_packets_.front();
    if (!framer_.ProcessPacket(*packet) &&
        framer_.error() == QUIC_DECRYPTION_FAILURE) {
      DVLOG(1) << ENDPOINT << "Unable to process undecryptable packet...";
      break;
    }
    DVLOG(1) << ENDPOINT << "Processed undecryptable packet!";
    delete packet;
    undecryptable_packets_.pop_front();
  }

  // Once forward secure encryption is in use, there will be no
  // new keys installed and hence any undecryptable packets will
  // never be able to be decrypted.
  if (encryption_level_ == ENCRYPTION_FORWARD_SECURE) {
    STLDeleteElements(&undecryptable_packets_);
  }
}

void QuicConnection::MaybeProcessRevivedPacket() {
  QuicFecGroup* group = GetFecGroup();
  if (!connected_ || group == NULL || !group->CanRevive()) {
    return;
  }
  QuicPacketHeader revived_header;
  char revived_payload[kMaxPacketSize];
  size_t len = group->Revive(&revived_header, revived_payload, kMaxPacketSize);
  revived_header.public_header.guid = guid_;
  revived_header.public_header.version_flag = false;
  revived_header.public_header.reset_flag = false;
  revived_header.fec_flag = false;
  revived_header.is_in_fec_group = NOT_IN_FEC_GROUP;
  revived_header.fec_group = 0;
  group_map_.erase(last_header_.fec_group);
  delete group;

  last_packet_revived_ = true;
  if (debug_visitor_) {
    debug_visitor_->OnRevivedPacket(revived_header,
                                    StringPiece(revived_payload, len));
  }

  ++stats_.packets_revived;
  framer_.ProcessRevivedPacket(&revived_header,
                               StringPiece(revived_payload, len));
}

QuicFecGroup* QuicConnection::GetFecGroup() {
  QuicFecGroupNumber fec_group_num = last_header_.fec_group;
  if (fec_group_num == 0) {
    return NULL;
  }
  if (group_map_.count(fec_group_num) == 0) {
    if (group_map_.size() >= kMaxFecGroups) {  // Too many groups
      if (fec_group_num < group_map_.begin()->first) {
        // The group being requested is a group we've seen before and deleted.
        // Don't recreate it.
        return NULL;
      }
      // Clear the lowest group number.
      delete group_map_.begin()->second;
      group_map_.erase(group_map_.begin());
    }
    group_map_[fec_group_num] = new QuicFecGroup();
  }
  return group_map_[fec_group_num];
}

void QuicConnection::SendConnectionClose(QuicErrorCode error) {
  SendConnectionCloseWithDetails(error, string());
}

void QuicConnection::SendConnectionClosePacket(QuicErrorCode error,
                                               const string& details) {
  DLOG(INFO) << ENDPOINT << "Force closing with error "
             << QuicUtils::ErrorToString(error) << " (" << error << ") "
             << details;
  QuicConnectionCloseFrame frame;
  frame.error_code = error;
  frame.error_details = details;
  UpdateSentPacketInfo(&frame.ack_frame.sent_info);
  received_packet_manager_.UpdateReceivedPacketInfo(
      &frame.ack_frame.received_info, clock_->ApproximateNow());

  SerializedPacket serialized_packet =
      packet_creator_.SerializeConnectionClose(&frame);

  // We need to update the sent entropy hash for all sent packets.
  sent_entropy_manager_.RecordPacketEntropyHash(
      serialized_packet.sequence_number,
      serialized_packet.entropy_hash);

  if (!WritePacket(encryption_level_,
                   serialized_packet.sequence_number,
                   serialized_packet.packet,
                   serialized_packet.retransmittable_frames != NULL ?
                       HAS_RETRANSMITTABLE_DATA : NO_RETRANSMITTABLE_DATA,
                   FORCE)) {
    delete serialized_packet.packet;
  }
}

void QuicConnection::SendConnectionCloseWithDetails(QuicErrorCode error,
                                                    const string& details) {
  if (!write_blocked_) {
    SendConnectionClosePacket(error, details);
  }
  CloseConnection(error, false);
}

void QuicConnection::CloseConnection(QuicErrorCode error, bool from_peer) {
  DCHECK(connected_);
  connected_ = false;
  visitor_->ConnectionClose(error, from_peer);
}

void QuicConnection::SendGoAway(QuicErrorCode error,
                                QuicStreamId last_good_stream_id,
                                const string& reason) {
  DLOG(INFO) << ENDPOINT << "Going away with error "
             << QuicUtils::ErrorToString(error)
             << " (" << error << ")";
  packet_generator_.AddControlFrame(
      QuicFrame(new QuicGoAwayFrame(error, last_good_stream_id, reason)));
}

void QuicConnection::CloseFecGroupsBefore(
    QuicPacketSequenceNumber sequence_number) {
  FecGroupMap::iterator it = group_map_.begin();
  while (it != group_map_.end()) {
    // If this is the current group or the group doesn't protect this packet
    // we can ignore it.
    if (last_header_.fec_group == it->first ||
        !it->second->ProtectsPacketsBefore(sequence_number)) {
      ++it;
      continue;
    }
    QuicFecGroup* fec_group = it->second;
    DCHECK(!fec_group->CanRevive());
    FecGroupMap::iterator next = it;
    ++next;
    group_map_.erase(it);
    delete fec_group;
    it = next;
  }
}

bool QuicConnection::HasQueuedData() const {
  return !queued_packets_.empty() || packet_generator_.HasQueuedFrames();
}

void QuicConnection::SetIdleNetworkTimeout(QuicTime::Delta timeout) {
  if (timeout < idle_network_timeout_) {
    idle_network_timeout_ = timeout;
    CheckForTimeout();
  } else {
     idle_network_timeout_ = timeout;
  }
}

void QuicConnection::SetOverallConnectionTimeout(QuicTime::Delta timeout) {
  if (timeout < overall_connection_timeout_) {
    overall_connection_timeout_ = timeout;
    CheckForTimeout();
  } else {
    overall_connection_timeout_ = timeout;
  }
}

bool QuicConnection::CheckForTimeout() {
  QuicTime now = clock_->ApproximateNow();
  QuicTime time_of_last_packet = std::max(time_of_last_received_packet_,
                                          time_of_last_sent_packet_);

  // |delta| can be < 0 as |now| is approximate time but |time_of_last_packet|
  // is accurate time. However, this should not change the behavior of
  // timeout handling.
  QuicTime::Delta delta = now.Subtract(time_of_last_packet);
  DVLOG(1) << ENDPOINT << "last packet "
           << time_of_last_packet.ToDebuggingValue()
           << " now:" << now.ToDebuggingValue()
           << " delta:" << delta.ToMicroseconds()
           << " network_timeout: " << idle_network_timeout_.ToMicroseconds();
  if (delta >= idle_network_timeout_) {
    DVLOG(1) << ENDPOINT << "Connection timedout due to no network activity.";
    SendConnectionClose(QUIC_CONNECTION_TIMED_OUT);
    return true;
  }

  // Next timeout delta.
  QuicTime::Delta timeout = idle_network_timeout_.Subtract(delta);

  if (!overall_connection_timeout_.IsInfinite()) {
    QuicTime::Delta connected_time = now.Subtract(creation_time_);
    DVLOG(1) << ENDPOINT << "connection time: "
             << connected_time.ToMilliseconds() << " overall timeout: "
             << overall_connection_timeout_.ToMilliseconds();
    if (connected_time >= overall_connection_timeout_) {
      DVLOG(1) << ENDPOINT <<
          "Connection timedout due to overall connection timeout.";
      SendConnectionClose(QUIC_CONNECTION_TIMED_OUT);
      return true;
    }

    // Take the min timeout.
    QuicTime::Delta connection_timeout =
        overall_connection_timeout_.Subtract(connected_time);
    if (connection_timeout < timeout) {
      timeout = connection_timeout;
    }
  }

  timeout_alarm_->Cancel();
  timeout_alarm_->Set(clock_->ApproximateNow().Add(timeout));
  return false;
}

}  // namespace net
