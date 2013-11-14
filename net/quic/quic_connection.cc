// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_connection.h"

#include <string.h>
#include <sys/types.h>
#include <algorithm>
#include <iterator>
#include <limits>
#include <memory>
#include <set>
#include <utility>

#include "base/logging.h"
#include "base/stl_util.h"
#include "net/base/net_errors.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/iovector.h"
#include "net/quic/quic_bandwidth.h"
#include "net/quic/quic_config.h"
#include "net/quic/quic_utils.h"

using base::hash_map;
using base::hash_set;
using base::StringPiece;
using std::list;
using std::make_pair;
using std::min;
using std::max;
using std::numeric_limits;
using std::vector;
using std::set;
using std::string;

int FLAGS_fake_packet_loss_percentage = 0;

// If true, then QUIC connections will bundle acks with any outgoing packet when
// an ack is being delayed. This is an optimization to reduce ack latency and
// packet count of pure ack packets.
bool FLAGS_bundle_ack_with_outgoing_packet = false;

namespace net {

class QuicDecrypter;
class QuicEncrypter;

namespace {

// The largest gap in packets we'll accept without closing the connection.
// This will likely have to be tuned.
const QuicPacketSequenceNumber kMaxPacketGap = 5000;

// We want to make sure if we get a nack packet which triggers several
// retransmissions, we don't queue up too many packets.  10 is TCP's default
// initial congestion window(RFC 6928).
const size_t kMaxRetransmissionsPerAck = kDefaultInitialWindow;

// TCP retransmits after 3 nacks.
// TODO(ianswett): Change to match TCP's rule of retransmitting once an ack
// at least 3 sequence numbers larger arrives.
const size_t kNumberOfNacksBeforeRetransmission = 3;

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
    connection_->OnRetransmissionTimeout();
    return QuicTime::Zero();
  }

 private:
  QuicConnection* connection_;
};

// This alarm will be scheduled any time a FEC-bearing packet is sent out.
// When the alarm goes off, the connection checks to see if the oldest packets
// have been acked, and removes them from the congestion window if not.
class AbandonFecAlarm : public QuicAlarm::Delegate {
 public:
  explicit AbandonFecAlarm(QuicConnection* connection)
      : connection_(connection) {
  }

  virtual QuicTime OnAlarm() OVERRIDE {
    return connection_->OnAbandonFecTimeout();
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
    connection_->WriteIfNotBlocked();
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

// Indicates if any of the frames are intended to be sent with FORCE.
// Returns true when one of the frames is a CONNECTION_CLOSE_FRAME.
net::QuicConnection::Force HasForcedFrames(
    const RetransmittableFrames* retransmittable_frames) {
  if (!retransmittable_frames) {
    return net::QuicConnection::NO_FORCE;
  }
  for (size_t i = 0; i < retransmittable_frames->frames().size(); ++i) {
    if (retransmittable_frames->frames()[i].type == CONNECTION_CLOSE_FRAME) {
      return net::QuicConnection::FORCE;
    }
  }
  return net::QuicConnection::NO_FORCE;
}

net::IsHandshake HasCryptoHandshake(
    const RetransmittableFrames* retransmittable_frames) {
  if (!retransmittable_frames) {
    return net::NOT_HANDSHAKE;
  }
  for (size_t i = 0; i < retransmittable_frames->frames().size(); ++i) {
    if (retransmittable_frames->frames()[i].type == STREAM_FRAME &&
        retransmittable_frames->frames()[i].stream_frame->stream_id ==
            kCryptoStreamId) {
      return net::IS_HANDSHAKE;
    }
  }
  return net::NOT_HANDSHAKE;
}

}  // namespace

#define ENDPOINT (is_server_ ? "Server: " : " Client: ")

QuicConnection::QuicConnection(QuicGuid guid,
                               IPEndPoint address,
                               QuicConnectionHelperInterface* helper,
                               QuicPacketWriter* writer,
                               bool is_server,
                               const QuicVersionVector& supported_versions)
    : framer_(supported_versions,
              helper->GetClock()->ApproximateNow(),
              is_server),
      helper_(helper),
      writer_(writer),
      encryption_level_(ENCRYPTION_NONE),
      clock_(helper->GetClock()),
      random_generator_(helper->GetRandomGenerator()),
      guid_(guid),
      peer_address_(address),
      largest_seen_packet_with_ack_(0),
      pending_version_negotiation_packet_(false),
      write_blocked_(false),
      ack_alarm_(helper->CreateAlarm(new AckAlarm(this))),
      retransmission_alarm_(helper->CreateAlarm(new RetransmissionAlarm(this))),
      abandon_fec_alarm_(helper->CreateAlarm(new AbandonFecAlarm(this))),
      send_alarm_(helper->CreateAlarm(new SendAlarm(this))),
      resume_writes_alarm_(helper->CreateAlarm(new SendAlarm(this))),
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
      sequence_number_of_last_inorder_packet_(0),
      congestion_manager_(clock_, kTCP),
      sent_packet_manager_(is_server, this),
      version_negotiation_state_(START_NEGOTIATION),
      consecutive_rto_count_(0),
      is_server_(is_server),
      connected_(true),
      received_truncated_ack_(false),
      address_migrating_(false) {
  DLOG(INFO) << ENDPOINT << "Created connection with guid: " << guid;
  timeout_alarm_->Set(clock_->ApproximateNow().Add(idle_network_timeout_));
  framer_.set_visitor(this);
  framer_.set_received_entropy_calculator(&received_packet_manager_);
}

QuicConnection::~QuicConnection() {
  STLDeleteElements(&undecryptable_packets_);
  STLDeleteValues(&group_map_);
  for (QueuedPacketList::iterator it = queued_packets_.begin();
       it != queued_packets_.end(); ++it) {
    delete it->packet;
  }
}

void QuicConnection::SetFromConfig(const QuicConfig& config) {
  DCHECK_LT(0u, config.server_max_packet_size());
  DCHECK_LT(0u, config.server_initial_congestion_window());
  SetIdleNetworkTimeout(config.idle_connection_state_lifetime());
  // Set the max packet length only when QUIC_VERSION_12 or later is supported,
  // with explicitly truncated acks.
  if (version() > QUIC_VERSION_11 && is_server_) {
    options()->max_packet_length = config.server_max_packet_size();
  }
  congestion_manager_.SetFromConfig(config, is_server_);
  // TODO(satyamshekhar): Set congestion control and ICSL also.
}

bool QuicConnection::SelectMutualVersion(
    const QuicVersionVector& available_versions) {
  // Try to find the highest mutual version by iterating over supported
  // versions, starting with the highest, and breaking out of the loop once we
  // find a matching version in the provided available_versions vector.
  const QuicVersionVector& supported_versions = framer_.supported_versions();
  for (size_t i = 0; i < supported_versions.size(); ++i) {
    const QuicVersion& version = supported_versions[i];
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
  DLOG(INFO) << ENDPOINT << "Received packet with mismatched version "
             << received_version;
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
        SendVersionNegotiationPacket();
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
  visitor_->OnSuccessfulVersionNegotiation(received_version);
  DLOG(INFO) << ENDPOINT << "version negotiated " << received_version;

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

  DLOG(INFO) << ENDPOINT << "negotiating version " << version();
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
        visitor_->OnSuccessfulVersionNegotiation(version());
      }
    } else {
      DCHECK(!header.public_header.version_flag);
      // If the client gets a packet without the version flag from the server
      // it should stop sending version since the version negotiation is done.
      packet_creator_.StopSendingVersion();
      version_negotiation_state_ = NEGOTIATED_VERSION;
      visitor_->OnSuccessfulVersionNegotiation(version());
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
  // Latch current least unacked sequence number. This allows us to reset the
  // retransmission and FEC abandonment timers conditionally below.
  QuicPacketSequenceNumber least_unacked_sent_before =
      sent_packet_manager_.GetLeastUnackedSentPacket();

  largest_seen_packet_with_ack_ = last_header_.packet_sequence_number;

  received_truncated_ack_ = version() <= QUIC_VERSION_11 ?
      incoming_ack.received_info.missing_packets.size() >=
          QuicFramer::GetMaxUnackedPackets(last_header_) :
      incoming_ack.received_info.is_truncated;

  received_packet_manager_.UpdatePacketInformationReceivedByPeer(incoming_ack);
  received_packet_manager_.UpdatePacketInformationSentByPeer(incoming_ack);
  // Possibly close any FecGroups which are now irrelevant.
  CloseFecGroupsBefore(incoming_ack.sent_info.least_unacked + 1);

  sent_entropy_manager_.ClearEntropyBefore(
      received_packet_manager_.least_packet_awaited_by_peer() - 1);

  retransmitted_nacked_packet_count_ = 0;
  sent_packet_manager_.OnIncomingAck(incoming_ack.received_info,
                                     received_truncated_ack_);

  // Get the updated least unacked sequence number.
  QuicPacketSequenceNumber least_unacked_sent_after =
      sent_packet_manager_.GetLeastUnackedSentPacket();

  // Used to set RTO and FEC alarms.
  QuicTime::Delta retransmission_delay =
      congestion_manager_.GetRetransmissionDelay(
          sent_packet_manager_.GetNumUnackedPackets(), 0);

  // If there are outstanding packets, and the least unacked sequence number
  // has increased after processing this latest AckFrame, then reschedule the
  // retransmission timer.
  if (sent_packet_manager_.HasUnackedPackets() &&
      least_unacked_sent_before < least_unacked_sent_after) {
    if (retransmission_alarm_->IsSet()) {
      retransmission_alarm_->Cancel();
    }
    retransmission_alarm_->Set(
        clock_->ApproximateNow().Add(retransmission_delay));
    consecutive_rto_count_ = 0;
  } else if (!sent_packet_manager_.HasUnackedPackets()) {
    retransmission_alarm_->Cancel();
  }

  // If there are outstanding FEC packets, and the least unacked sequence number
  // has increased after processing this latest AckFrame, then reschedule the
  // abandon FEC timer.
  abandon_fec_alarm_->Cancel();
  if (sent_packet_manager_.HasUnackedFecPackets() &&
      least_unacked_sent_before < least_unacked_sent_after) {
    abandon_fec_alarm_->Set(clock_->ApproximateNow().Add(retransmission_delay));
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
  if (version() <= QUIC_VERSION_11) {
    DCHECK_LE(incoming_ack.received_info.missing_packets.size(),
              QuicFramer::GetMaxUnackedPackets(last_header_));
  }

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
                << " which is greater than largest observed: "
                << incoming_ack.received_info.largest_observed;
    return false;
  }

  if (!incoming_ack.received_info.missing_packets.empty() &&
      *incoming_ack.received_info.missing_packets.begin() <
      received_packet_manager_.least_packet_awaited_by_peer()) {
    DLOG(ERROR) << ENDPOINT << "Peer sent missing packet: "
                << *incoming_ack.received_info.missing_packets.begin()
                << " which is smaller than least_packet_awaited_by_peer_: "
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
  DLOG(INFO) << ENDPOINT << "Connection " << guid() << " closed with error "
             << QuicUtils::ErrorToString(frame.error_code)
             << " " << frame.error_details;
  last_close_frames_.push_back(frame);
  return connected_;
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

  // If we are missing any packets from the peer, then we want to ack
  // immediately.  We need to check both before and after we process the
  // current packet because we want to ack immediately when we discover
  // a missing packet AND when we receive the last missing packet.
  bool send_ack_immediately =
      received_packet_manager_.GetNumMissingPackets() != 0;

  // Ensure the visitor can process the stream frames before recording and
  // processing the rest of the packet.
  if (last_stream_frames_.empty() ||
      visitor_->OnStreamFrames(last_stream_frames_)) {
    received_packet_manager_.RecordPacketReceived(
        last_header_, time_of_last_received_packet_);
    for (size_t i = 0; i < last_stream_frames_.size(); ++i) {
      stats_.stream_bytes_received += last_stream_frames_[i].data.length();
    }
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
  if (!last_close_frames_.empty()) {
    CloseConnection(last_close_frames_[0].error_code, true);
    DCHECK(!connected_);
  }

  if (received_packet_manager_.GetNumMissingPackets() != 0) {
    send_ack_immediately = true;
  }

  MaybeSendInResponseToPacket(send_ack_immediately,
                              last_packet_should_instigate_ack);

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
      !last_ack_frames_.back().received_info.missing_packets.empty()) {
    return sent_packet_manager_.GetLeastUnackedSentPacket() >
        *last_ack_frames_.back().received_info.missing_packets.begin();
  }
  return false;
}

void QuicConnection::MaybeSendInResponseToPacket(
    bool send_ack_immediately,
    bool last_packet_should_instigate_ack) {
  // |include_ack| is false since we decide about ack bundling below.
  ScopedPacketBundler bundler(this, false);

  if (last_packet_should_instigate_ack) {
    // In general, we ack every second packet.  When we don't ack the first
    // packet, we set the delayed ack alarm.  Thus, if the ack alarm is set
    // then we know this is the second packet, and we should send an ack.
    if (send_ack_immediately || ack_alarm_->IsSet()) {
      SendAck();
      DCHECK(!ack_alarm_->IsSet());
    } else {
      ack_alarm_->Set(clock_->ApproximateNow().Add(
          congestion_manager_.DelayedAckTime()));
      DVLOG(1) << "Ack timer set; next packet or timer will trigger ACK.";
    }
  }

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
}

void QuicConnection::SendVersionNegotiationPacket() {
  scoped_ptr<QuicEncryptedPacket> version_packet(
      packet_creator_.SerializeVersionNegotiationPacket(
          framer_.supported_versions()));
  // TODO(satyamshekhar): implement zero server state negotiation.
  WriteResult result =
      writer_->WritePacket(version_packet->data(), version_packet->length(),
                           self_address().address(), peer_address(), this);
  if (result.status == WRITE_STATUS_BLOCKED) {
    write_blocked_ = true;
  }
  if (result.status == WRITE_STATUS_OK ||
      (result.status == WRITE_STATUS_BLOCKED &&
       writer_->IsWriteBlockedDataBuffered())) {
    pending_version_negotiation_packet_ = false;
    return;
  }
  if (result.status == WRITE_STATUS_ERROR) {
    // We can't send an error as the socket is presumably borked.
    CloseConnection(QUIC_PACKET_WRITE_ERROR, false);
  }
  pending_version_negotiation_packet_ = true;
}

QuicConsumedData QuicConnection::SendStreamDataInner(
    QuicStreamId id,
    const IOVector& data,
    QuicStreamOffset offset,
    bool fin,
    QuicAckNotifier* notifier) {
  // TODO(ianswett): Further improve sending by passing the iovec down
  // instead of batching into multiple stream frames in a single packet.

  // Opportunistically bundle an ack with this outgoing packet.
  ScopedPacketBundler ack_bundler(this, true);
  size_t bytes_written = 0;
  bool fin_consumed = false;

  for (size_t i = 0; i < data.Size(); ++i) {
    bool send_fin = fin && (i == data.Size() - 1);
    if (!send_fin && data.iovec()[i].iov_len == 0) {
      LOG(DFATAL) << "Attempt to send empty stream frame";
    }

    StringPiece data_piece(static_cast<char*>(data.iovec()[i].iov_base),
                           data.iovec()[i].iov_len);
    int currentOffset = offset + bytes_written;
    QuicConsumedData consumed_data =
        packet_generator_.ConsumeData(id,
                                      data_piece,
                                      currentOffset,
                                      send_fin,
                                      notifier);

    DCHECK_LE(consumed_data.bytes_consumed, numeric_limits<uint32>::max());
    bytes_written += consumed_data.bytes_consumed;
    fin_consumed = consumed_data.fin_consumed;
    // If no bytes were consumed, bail now, because the stream can not write
    // more data.
    if (consumed_data.bytes_consumed < data.iovec()[i].iov_len) {
      break;
    }
  }
  // Handle the 0 byte write properly.
  if (data.Empty()) {
    DCHECK(fin);
    QuicConsumedData consumed_data = packet_generator_.ConsumeData(
        id, StringPiece(), offset, fin, NULL);
    fin_consumed = consumed_data.fin_consumed;
  }

  stats_.stream_bytes_sent += bytes_written;
  return QuicConsumedData(bytes_written, fin_consumed);
}

QuicConsumedData QuicConnection::SendStreamData(QuicStreamId id,
                                                const IOVector& data,
                                                QuicStreamOffset offset,
                                                bool fin) {
  return SendStreamDataInner(id, data, offset, fin, NULL);
}

QuicConsumedData QuicConnection::SendStreamDataAndNotifyWhenAcked(
    QuicStreamId id,
    const IOVector& data,
    QuicStreamOffset offset,
    bool fin,
    QuicAckNotifier::DelegateInterface* delegate) {
  if (!fin && data.Empty()) {
    LOG(DFATAL) << "Attempt to send empty stream frame";
  }

  // This notifier will be owned by the AckNotifierManager (or deleted below if
  // no data was consumed).
  QuicAckNotifier* notifier = new QuicAckNotifier(delegate);
  QuicConsumedData consumed_data =
      SendStreamDataInner(id, data, offset, fin, notifier);

  if (consumed_data.bytes_consumed == 0) {
    // No data was consumed, delete the notifier.
    delete notifier;
  }

  return consumed_data;
}

void QuicConnection::SendRstStream(QuicStreamId id,
                                   QuicRstStreamErrorCode error) {
  LOG(INFO) << "Sending RST_STREAM: " << id << " code: " << error;
  // Opportunistically bundle an ack with this outgoing packet.
  ScopedPacketBundler ack_bundler(this, true);
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

  WritePendingRetransmissions();

  IsHandshake pending_handshake = visitor_->HasPendingHandshake() ?
      IS_HANDSHAKE : NOT_HANDSHAKE;
  // Sending queued packets may have caused the socket to become write blocked,
  // or the congestion manager to prohibit sending.  If we've sent everything
  // we had queued and we're still not blocked, let the visitor know it can
  // write more.
  if (CanWrite(NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA,
               pending_handshake)) {
    // Set |include_ack| to false in bundler; ack inclusion happens elsewhere.
    scoped_ptr<ScopedPacketBundler> bundler(
        new ScopedPacketBundler(this, false));
    bool all_bytes_written = visitor_->OnCanWrite();
    bundler.reset();
    // After the visitor writes, it may have caused the socket to become write
    // blocked or the congestion manager to prohibit sending, so check again.
    pending_handshake = visitor_->HasPendingHandshake() ? IS_HANDSHAKE
                                                        : NOT_HANDSHAKE;
    if (!all_bytes_written && !resume_writes_alarm_->IsSet() &&
        CanWrite(NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA,
                 pending_handshake)) {
      // We're not write blocked, but some stream didn't write out all of its
      // bytes.  Register for 'immediate' resumption so we'll keep writing after
      // other quic connections have had a chance to use the socket.
      resume_writes_alarm_->Set(clock_->ApproximateNow());
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

  if (pending_version_negotiation_packet_) {
    SendVersionNegotiationPacket();
  }

  QueuedPacketList::iterator packet_iterator = queued_packets_.begin();
  while (!write_blocked_ && packet_iterator != queued_packets_.end()) {
    if (WritePacket(packet_iterator->encryption_level,
                    packet_iterator->sequence_number,
                    packet_iterator->packet,
                    packet_iterator->transmission_type,
                    packet_iterator->retransmittable,
                    packet_iterator->handshake,
                    packet_iterator->forced)) {
      packet_iterator = queued_packets_.erase(packet_iterator);
    } else {
      // Continue, because some queued packets may still be writable.
      // This can happen if a retransmit send fail.
      ++packet_iterator;
    }
  }

  return !write_blocked_;
}

void QuicConnection::WritePendingRetransmissions() {
  // Keep writing as long as there's a pending retransmission which can be
  // written.
  while (sent_packet_manager_.HasPendingRetransmissions()) {
    const QuicSentPacketManager::PendingRetransmission pending =
        sent_packet_manager_.NextPendingRetransmission();
    if (HasForcedFrames(&pending.retransmittable_frames) == NO_FORCE &&
        !CanWrite(pending.transmission_type, HAS_RETRANSMITTABLE_DATA,
                  HasCryptoHandshake(&pending.retransmittable_frames))) {
      break;
    }

    // Re-packetize the frames with a new sequence number for retransmission.
    // Retransmitted data packets do not use FEC, even when it's enabled.
    // Retransmitted packets use the same sequence number length as the
    // original.
    // Flush the packet creator before making a new packet.
    // TODO(ianswett): Implement ReserializeAllFrames as a separate path that
    // does not require the creator to be flushed.
    Flush();
    SerializedPacket serialized_packet = packet_creator_.ReserializeAllFrames(
        pending.retransmittable_frames.frames(),
        pending.sequence_number_length);

    DLOG(INFO) << ENDPOINT << "Retransmitting " << pending.sequence_number
               << " as " << serialized_packet.sequence_number;
    if (debug_visitor_) {
      debug_visitor_->OnPacketRetransmitted(
          pending.sequence_number, serialized_packet.sequence_number);
    }
    sent_packet_manager_.OnRetransmittedPacket(
        pending.sequence_number, serialized_packet.sequence_number);

    SendOrQueuePacket(pending.retransmittable_frames.encryption_level(),
                      serialized_packet.sequence_number,
                      serialized_packet.packet,
                      serialized_packet.entropy_hash,
                      pending.transmission_type,
                      HAS_RETRANSMITTABLE_DATA,
                      HasCryptoHandshake(
                          serialized_packet.retransmittable_frames),
                      HasForcedFrames(
                          serialized_packet.retransmittable_frames));
  }
}

void QuicConnection::RetransmitUnackedPackets(
    RetransmissionType retransmission_type) {
  SequenceNumberSet unacked_packets =
      sent_packet_manager_.GetUnackedPackets();
  if (unacked_packets.empty()) {
    return;
  }

  for (SequenceNumberSet::const_iterator unacked_it = unacked_packets.begin();
       unacked_it != unacked_packets.end(); ++unacked_it) {
    if (!sent_packet_manager_.HasRetransmittableFrames(*unacked_it)) {
      continue;
    }
    const RetransmittableFrames& frames =
        sent_packet_manager_.GetRetransmittableFrames(*unacked_it);
    if (retransmission_type == ALL_PACKETS ||
        frames.encryption_level() == ENCRYPTION_INITIAL) {
      // TODO(satyamshekhar): Think about congestion control here.
      // Specifically, about the retransmission count of packets being sent
      // proactively to achieve 0 (minimal) RTT.
      RetransmitPacket(*unacked_it, NACK_RETRANSMISSION);
    }
  }
}

void QuicConnection::RetransmitPacket(
    QuicPacketSequenceNumber sequence_number,
    TransmissionType transmission_type) {
  DCHECK(sent_packet_manager_.IsUnacked(sequence_number));

  // TODO(pwestin): Need to fix potential issue with FEC and a 1 packet
  // congestion window see b/8331807 for details.
  congestion_manager_.OnPacketAbandoned(sequence_number);

  // If we have received an ACK for an old version of this packet, then
  // we should not retransmit the data.
  if (!sent_packet_manager_.MarkForRetransmission(sequence_number,
                                                  transmission_type)) {
    sent_packet_manager_.DiscardUnackedPacket(sequence_number);
    return;
  }

  WriteIfNotBlocked();
}

bool QuicConnection::ShouldGeneratePacket(
    TransmissionType transmission_type,
    HasRetransmittableData retransmittable,
    IsHandshake handshake) {
  // We should serialize handshake packets immediately to ensure that they
  // end up sent at the right encryption level.
  if (handshake == IS_HANDSHAKE) {
    return true;
  }

  return CanWrite(transmission_type, retransmittable, handshake);
}

bool QuicConnection::CanWrite(TransmissionType transmission_type,
                              HasRetransmittableData retransmittable,
                              IsHandshake handshake) {
  // This check assumes that if the send alarm is set, it applies equally to all
  // types of transmissions.
  if (write_blocked_ || send_alarm_->IsSet()) {
    return false;
  }

  QuicTime now = clock_->Now();
  QuicTime::Delta delay = congestion_manager_.TimeUntilSend(
      now, transmission_type, retransmittable, handshake);
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

void QuicConnection::SetupRetransmission(
    QuicPacketSequenceNumber sequence_number,
    EncryptionLevel level) {
  if (!sent_packet_manager_.HasRetransmittableFrames(sequence_number)) {
    DVLOG(1) << ENDPOINT << "Will not retransmit packet " << sequence_number;
    return;
  }

  // Do not set the retransmission alarm if we're already handling one, since
  // it will be reset when OnRetransmissionTimeout completes.
  if (retransmission_alarm_->IsSet()) {
    return;
  }

  QuicTime::Delta retransmission_delay =
      congestion_manager_.GetRetransmissionDelay(
          sent_packet_manager_.GetNumUnackedPackets(), consecutive_rto_count_);
  retransmission_alarm_->Set(
      clock_->ApproximateNow().Add(retransmission_delay));
}

void QuicConnection::SetupAbandonFecTimer(
    QuicPacketSequenceNumber sequence_number) {
  if (abandon_fec_alarm_->IsSet()) {
    return;
  }
  QuicTime::Delta retransmission_delay =
      congestion_manager_.GetRetransmissionDelay(
          sent_packet_manager_.GetNumUnackedPackets(), consecutive_rto_count_);
  abandon_fec_alarm_->Set(clock_->ApproximateNow().Add(retransmission_delay));
}

bool QuicConnection::WritePacket(EncryptionLevel level,
                                 QuicPacketSequenceNumber sequence_number,
                                 QuicPacket* packet,
                                 TransmissionType transmission_type,
                                 HasRetransmittableData retransmittable,
                                 IsHandshake handshake,
                                 Force forced) {
  if (ShouldDiscardPacket(level, sequence_number, retransmittable)) {
    delete packet;
    return true;
  }

  // If we're write blocked, we know we can't write.
  if (write_blocked_) {
    return false;
  }

  // If we are not forced and we can't write, then simply return false;
  if (forced == NO_FORCE &&
      !CanWrite(transmission_type, retransmittable, handshake)) {
    return false;
  }

  // Some encryption algorithms require the packet sequence numbers not be
  // repeated.
  DCHECK_LE(sequence_number_of_last_inorder_packet_, sequence_number);
  // Only increase this when packets have not been queued.  Once they're queued
  // due to a write block, there is the chance of sending forced and other
  // higher priority packets out of order.
  if (queued_packets_.empty()) {
    sequence_number_of_last_inorder_packet_ = sequence_number;
  }

  scoped_ptr<QuicEncryptedPacket> encrypted(
      framer_.EncryptPacket(level, sequence_number, *packet));
  if (encrypted.get() == NULL) {
    LOG(DFATAL) << ENDPOINT << "Failed to encrypt packet number "
                << sequence_number;
    CloseConnection(QUIC_ENCRYPTION_FAILURE, false);
    return false;
  }

  if (encrypted->length() > options()->max_packet_length) {
    LOG(DFATAL) << ENDPOINT
                << "Writing an encrypted packet larger than max_packet_length:"
                << options()->max_packet_length;
  }
  DLOG(INFO) << ENDPOINT << "Sending packet number " << sequence_number
             << " : " << (packet->is_fec_packet() ? "FEC " :
                 (retransmittable == HAS_RETRANSMITTABLE_DATA
                      ? "data bearing " : " ack only "))
             << ", encryption level: "
             << QuicUtils::EncryptionLevelToString(level)
             << ", length:" << packet->length() << ", encrypted length:"
             << encrypted->length();
  DVLOG(2) << ENDPOINT << "packet(" << sequence_number << "): " << std::endl
           << QuicUtils::StringToHexASCIIDump(packet->AsStringPiece());

  DCHECK(encrypted->length() <= kMaxPacketSize)
      << "Packet " << sequence_number << " will not be read; too large: "
      << packet->length() << " " << encrypted->length() << " "
      << " forced: " << (forced == FORCE ? "yes" : "no");

  DCHECK(pending_write_.get() == NULL);
  pending_write_.reset(new PendingWrite(sequence_number, transmission_type,
                                        retransmittable, level,
                                        packet->is_fec_packet(),
                                        packet->length()));

  WriteResult result =
      writer_->WritePacket(encrypted->data(), encrypted->length(),
                           self_address().address(), peer_address(), this);
  if (result.error_code == ERR_IO_PENDING) {
    DCHECK_EQ(WRITE_STATUS_BLOCKED, result.status);
  }
  if (debug_visitor_) {
    // Pass the write result to the visitor.
    debug_visitor_->OnPacketSent(sequence_number, level, *encrypted, result);
  }
  if (result.status == WRITE_STATUS_BLOCKED) {
    // TODO(satyashekhar): It might be more efficient (fewer system calls), if
    // all connections share this variable i.e this becomes a part of
    // PacketWriterInterface.
    write_blocked_ = true;
    // If the socket buffers the the data, then the packet should not
    // be queued and sent again, which would result in an unnecessary
    // duplicate packet being sent.  The helper must call OnPacketSent
    // when the packet is actually sent.
    if (writer_->IsWriteBlockedDataBuffered()) {
      delete packet;
      return true;
    }
    pending_write_.reset();
    return false;
  }

  if (OnPacketSent(result)) {
    delete packet;
    return true;
  }
  return false;
}

bool QuicConnection::ShouldDiscardPacket(
    EncryptionLevel level,
    QuicPacketSequenceNumber sequence_number,
    HasRetransmittableData retransmittable) {
  if (!connected_) {
    DLOG(INFO) << ENDPOINT
               << "Not sending packet as connection is disconnected.";
    return true;
  }

  if (encryption_level_ == ENCRYPTION_FORWARD_SECURE &&
      level == ENCRYPTION_NONE) {
    // Drop packets that are NULL encrypted since the peer won't accept them
    // anymore.
    DLOG(INFO) << ENDPOINT << "Dropping packet: " << sequence_number
               << " since the packet is NULL encrypted.";
    sent_packet_manager_.DiscardUnackedPacket(sequence_number);
    return true;
  }

  if (retransmittable == HAS_RETRANSMITTABLE_DATA) {
    if (!sent_packet_manager_.IsUnacked(sequence_number)) {
      // This is a crazy edge case, but if we retransmit a packet,
      // (but have to queue it for some reason) then receive an ack
      // for the previous transmission (but not the retransmission)
      // then receive a truncated ACK which causes us to raise the
      // high water mark, all before we're able to send the packet
      // then we can simply drop it.
      DLOG(INFO) << ENDPOINT << "Dropping packet: " << sequence_number
                 << " since it has already been acked.";
      return true;
    }

    if (sent_packet_manager_.IsPreviousTransmission(sequence_number)) {
      // If somehow we have already retransmitted this packet *before*
      // we actually send it for the first time (I think this is probably
      // impossible in the real world), then don't bother sending it.
      // We don't want to call DiscardUnackedPacket because in this case
      // the peer has not yet ACK'd the data.  We need the subsequent
      // retransmission to be sent.
      DLOG(INFO) << ENDPOINT << "Dropping packet: " << sequence_number
          << " since it has already been retransmitted.";
      return true;
    }

    if (!sent_packet_manager_.HasRetransmittableFrames(sequence_number)) {
      DLOG(INFO) << ENDPOINT << "Dropping packet: " << sequence_number
                 << " since a previous transmission has been acked.";
      sent_packet_manager_.DiscardUnackedPacket(sequence_number);
      return true;
    }
  }

  return false;
}

bool QuicConnection::OnPacketSent(WriteResult result) {
  DCHECK_NE(WRITE_STATUS_BLOCKED, result.status);
  if (pending_write_.get() == NULL) {
    LOG(DFATAL) << "OnPacketSent called without a pending write.";
    return false;
  }

  QuicPacketSequenceNumber sequence_number = pending_write_->sequence_number;
  TransmissionType transmission_type  = pending_write_->transmission_type;
  HasRetransmittableData retransmittable = pending_write_->retransmittable;
  EncryptionLevel level = pending_write_->level;
  bool is_fec_packet = pending_write_->is_fec_packet;
  size_t length = pending_write_->length;
  pending_write_.reset();

  if (result.status == WRITE_STATUS_ERROR) {
    DLOG(INFO) << "Write failed with error code: " << result.error_code;
    // We can't send an error as the socket is presumably borked.
    CloseConnection(QUIC_PACKET_WRITE_ERROR, false);
    return false;
  }

  QuicTime now = clock_->Now();
  if (transmission_type == NOT_RETRANSMISSION) {
    time_of_last_sent_packet_ = now;
  }
  DVLOG(1) << ENDPOINT << "time of last sent packet: "
           << now.ToDebuggingValue();

  // Set the retransmit alarm only when we have sent the packet to the client
  // and not when it goes to the pending queue, otherwise we will end up adding
  // an entry to retransmission_timeout_ every time we attempt a write.
  if (retransmittable == HAS_RETRANSMITTABLE_DATA) {
    SetupRetransmission(sequence_number, level);
  } else if (is_fec_packet) {
    SetupAbandonFecTimer(sequence_number);
  }

  // TODO(ianswett): Change the sequence number length and other packet creator
  // options by a more explicit API than setting a struct value directly.
  packet_creator_.UpdateSequenceNumberLength(
      received_packet_manager_.least_packet_awaited_by_peer(),
      congestion_manager_.BandwidthEstimate().ToBytesPerPeriod(
          congestion_manager_.SmoothedRtt()));

  congestion_manager_.OnPacketSent(sequence_number, now, length,
                                   transmission_type, retransmittable);

  stats_.bytes_sent += result.bytes_written;
  ++stats_.packets_sent;

  if (transmission_type == NACK_RETRANSMISSION ||
      transmission_type == RTO_RETRANSMISSION) {
    stats_.bytes_retransmitted += result.bytes_written;
    ++stats_.packets_retransmitted;
  }

  return true;
}

bool QuicConnection::OnSerializedPacket(
    const SerializedPacket& serialized_packet) {
  if (serialized_packet.retransmittable_frames) {
    serialized_packet.retransmittable_frames->
        set_encryption_level(encryption_level_);
  }
  sent_packet_manager_.OnSerializedPacket(serialized_packet,
                                          clock_->ApproximateNow());
  // The TransmissionType is NOT_RETRANSMISSION because all retransmissions
  // serialize packets and invoke SendOrQueuePacket directly.
  return SendOrQueuePacket(encryption_level_,
                           serialized_packet.sequence_number,
                           serialized_packet.packet,
                           serialized_packet.entropy_hash,
                           NOT_RETRANSMISSION,
                           serialized_packet.retransmittable_frames != NULL ?
                               HAS_RETRANSMITTABLE_DATA :
                               NO_RETRANSMITTABLE_DATA,
                           HasCryptoHandshake(
                               serialized_packet.retransmittable_frames),
                           HasForcedFrames(
                               serialized_packet.retransmittable_frames));
}

QuicPacketSequenceNumber QuicConnection::GetNextPacketSequenceNumber() {
  return packet_creator_.sequence_number() + 1;
}

void QuicConnection::OnPacketNacked(QuicPacketSequenceNumber sequence_number,
                                    size_t nack_count) {
  if (nack_count >= kNumberOfNacksBeforeRetransmission &&
      retransmitted_nacked_packet_count_ < kMaxRetransmissionsPerAck) {
    ++retransmitted_nacked_packet_count_;
    RetransmitPacket(sequence_number, NACK_RETRANSMISSION);
  }
}

bool QuicConnection::SendOrQueuePacket(EncryptionLevel level,
                                       QuicPacketSequenceNumber sequence_number,
                                       QuicPacket* packet,
                                       QuicPacketEntropyHash entropy_hash,
                                       TransmissionType transmission_type,
                                       HasRetransmittableData retransmittable,
                                       IsHandshake handshake,
                                       Force forced) {
  sent_entropy_manager_.RecordPacketEntropyHash(sequence_number, entropy_hash);
  if (!WritePacket(level, sequence_number, packet,
                   transmission_type, retransmittable, handshake, forced)) {
    queued_packets_.push_back(QueuedPacket(sequence_number, packet, level,
                                           transmission_type, retransmittable,
                                           handshake, forced));
    return false;
  }
  return true;
}

void QuicConnection::UpdateSentPacketInfo(SentPacketInfo* sent_info) {
  sent_info->least_unacked = sent_packet_manager_.GetLeastUnackedSentPacket();
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
    DVLOG(1) << ENDPOINT << "Sending feedback: "
             << outgoing_congestion_feedback_;
    send_feedback = true;
  }

  packet_generator_.SetShouldSendAck(send_feedback);
}

void QuicConnection::OnRetransmissionTimeout() {
  if (!sent_packet_manager_.HasUnackedPackets()) {
    return;
  }

  // TODO(ianswett): When an RTO fires, but the connection has not been
  // established as forward secure, re-send the client hello first.
  ++stats_.rto_count;
  ++consecutive_rto_count_;

  // Attempt to send all the unacked packets when the RTO fires, let the
  // congestion manager decide how many to send immediately and the remaining
  // packets will be queued for future sending.
  SequenceNumberSet unacked_packets =
      sent_packet_manager_.GetUnackedPackets();
  DLOG(INFO) << "OnRetransmissionTimeout() fired with "
             << unacked_packets.size() << " unacked packets.";

  // Abandon all unacked packets to ensure the congestion window
  // opens up before we attempt to retransmit the packet.
  for (SequenceNumberSet::const_iterator it = unacked_packets.begin();
       it != unacked_packets.end(); ++it) {
    congestion_manager_.OnPacketAbandoned(*it);
  }

  // Retransmit any packet with retransmittable frames.
  for (SequenceNumberSet::const_iterator it = unacked_packets.begin();
       it != unacked_packets.end(); ++it) {
    if (sent_packet_manager_.IsUnacked(*it) &&
        sent_packet_manager_.HasRetransmittableFrames(*it)) {
      RetransmitPacket(*it, RTO_RETRANSMISSION);
    }
  }

  // If the data from all unacked packets had been acked, then no packets will
  // have been retransmitted.  If we had been congestion blocked then we might
  // have data ready to send, so we should attempt to send it now.  If we don't
  // send now, and we never receive an ack from the peer, we may hang.
  if (!retransmission_alarm_->IsSet()) {
    WriteIfNotBlocked();
  }
}

QuicTime QuicConnection::OnAbandonFecTimeout() {
  // Abandon all the FEC packets older than the current RTO, then reschedule
  // the alarm if there are more pending fec packets.
  QuicTime::Delta retransmission_delay =
      congestion_manager_.GetRetransmissionDelay(
          sent_packet_manager_.GetNumUnackedPackets(), consecutive_rto_count_);
  QuicTime max_send_time =
      clock_->ApproximateNow().Subtract(retransmission_delay);
  bool abandoned_packet = false;
  while (sent_packet_manager_.HasUnackedFecPackets()) {
    QuicPacketSequenceNumber oldest_unacked_fec =
        sent_packet_manager_.GetLeastUnackedFecPacket();
    QuicTime fec_sent_time =
        sent_packet_manager_.GetFecSentTime(oldest_unacked_fec);
    if (fec_sent_time > max_send_time) {
      return fec_sent_time.Add(retransmission_delay);
    }
    abandoned_packet = true;
    sent_packet_manager_.DiscardFecPacket(oldest_unacked_fec);
    congestion_manager_.OnPacketAbandoned(oldest_unacked_fec);
  }
  if (abandoned_packet) {
    // If a packet was abandoned, then the congestion window may have
    // opened up, so attempt to write.
    WriteIfNotBlocked();
  }
  return QuicTime::Zero();
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

void QuicConnection::SendConnectionCloseWithDetails(QuicErrorCode error,
                                                    const string& details) {
  if (!write_blocked_) {
    SendConnectionClosePacket(error, details);
  }
  CloseConnection(error, false);
}

void QuicConnection::SendConnectionClosePacket(QuicErrorCode error,
                                               const string& details) {
  DLOG(INFO) << ENDPOINT << "Force closing " << guid() << " with error "
             << QuicUtils::ErrorToString(error) << " (" << error << ") "
             << details;
  ScopedPacketBundler ack_bundler(this, version() > QUIC_VERSION_11);
  QuicConnectionCloseFrame* frame = new QuicConnectionCloseFrame();
  frame->error_code = error;
  frame->error_details = details;
  UpdateSentPacketInfo(&frame->ack_frame.sent_info);
  received_packet_manager_.UpdateReceivedPacketInfo(
      &frame->ack_frame.received_info, clock_->ApproximateNow());
  packet_generator_.AddControlFrame(QuicFrame(frame));
  Flush();
}

void QuicConnection::CloseConnection(QuicErrorCode error, bool from_peer) {
  DCHECK(connected_);
  connected_ = false;
  visitor_->OnConnectionClosed(error, from_peer);
  // Cancel the alarms so they don't trigger any action now that the
  // connection is closed.
  ack_alarm_->Cancel();
  resume_writes_alarm_->Cancel();
  retransmission_alarm_->Cancel();
  send_alarm_->Cancel();
  timeout_alarm_->Cancel();
}

void QuicConnection::SendGoAway(QuicErrorCode error,
                                QuicStreamId last_good_stream_id,
                                const string& reason) {
  DLOG(INFO) << ENDPOINT << "Going away with error "
             << QuicUtils::ErrorToString(error)
             << " (" << error << ")";

  // Opportunistically bundle an ack with this outgoing packet.
  ScopedPacketBundler ack_bundler(this, true);
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

void QuicConnection::Flush() {
  packet_generator_.FlushAllQueuedFrames();
}

bool QuicConnection::HasQueuedData() const {
  return pending_version_negotiation_packet_ ||
      !queued_packets_.empty() || packet_generator_.HasQueuedFrames();
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

QuicConnection::ScopedPacketBundler::ScopedPacketBundler(
    QuicConnection* connection,
    bool include_ack)
    : connection_(connection),
      already_in_batch_mode_(connection->packet_generator_.InBatchMode()) {
  // Move generator into batch mode. If caller wants us to include an ack,
  // check the delayed-ack timer to see if there's ack info to be sent.
  if (!already_in_batch_mode_) {
    DVLOG(1) << "Entering Batch Mode.";
    connection_->packet_generator_.StartBatchOperations();
  }
  if (FLAGS_bundle_ack_with_outgoing_packet &&
      include_ack && connection_->ack_alarm_->IsSet()) {
    DVLOG(1) << "Bundling ack with outgoing packet.";
    connection_->SendAck();
  }
}

QuicConnection::ScopedPacketBundler::~ScopedPacketBundler() {
  // If we changed the generator's batch state, restore original batch state.
  if (!already_in_batch_mode_) {
    DVLOG(1) << "Leaving Batch Mode.";
    connection_->packet_generator_.FinishBatchOperations();
  }
  DCHECK_EQ(already_in_batch_mode_,
            connection_->packet_generator_.InBatchMode());
}

}  // namespace net
