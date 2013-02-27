// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_connection.h"

#include <algorithm>

#include "base/logging.h"
#include "base/stl_util.h"
#include "net/base/net_errors.h"
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

// TODO(pwestin): kDefaultTimeoutUs is in int64.
int32 kNegotiatedTimeoutUs = kDefaultTimeoutUs;

namespace {

// The largest gap in packets we'll accept without closing the connection.
// This will likely have to be tuned.
const QuicPacketSequenceNumber kMaxPacketGap = 5000;

// The maximum number of nacks which can be transmitted in a single ack packet
// without exceeding kMaxPacketSize.
// TODO(satyamshekhar): Get rid of magic numbers and move this to protocol.h
// 16 - Min ack frame size.
// 16 - Crypto hash for integrity. Not a static value. Use
// QuicEncrypter::GetMaxPlaintextSize.
const QuicPacketSequenceNumber kMaxUnackedPackets =
    (kMaxPacketSize - kPacketHeaderSize - 16 - 16) / kSequenceNumberSize;

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
const int kMaxPacketsPerRetransmissionAlarm = 10;

// Named constant for WritePacket()
const bool kForce = true;
// Named constant for CanWrite().
const bool kIsRetransmission = true;

bool Near(QuicPacketSequenceNumber a, QuicPacketSequenceNumber b) {
  QuicPacketSequenceNumber delta = (a > b) ? a - b : b - a;
  return delta <= kMaxPacketGap;
}

}  // namespace

QuicConnection::QuicConnection(QuicGuid guid,
                               IPEndPoint address,
                               QuicConnectionHelperInterface* helper)
    : helper_(helper),
      framer_(QuicDecrypter::Create(kNULL), QuicEncrypter::Create(kNULL)),
      clock_(helper->GetClock()),
      random_generator_(helper->GetRandomGenerator()),
      guid_(guid),
      peer_address_(address),
      largest_seen_packet_with_ack_(0),
      peer_largest_observed_packet_(0),
      least_packet_awaited_by_peer_(1),
      peer_least_packet_awaiting_ack_(0),
      handling_retransmission_timeout_(false),
      write_blocked_(false),
      debug_visitor_(NULL),
      packet_creator_(guid_, &framer_, random_generator_),
      packet_generator_(this, &packet_creator_),
      timeout_(QuicTime::Delta::FromMicroseconds(kDefaultTimeoutUs)),
      time_of_last_received_packet_(clock_->ApproximateNow()),
      time_of_last_sent_packet_(clock_->ApproximateNow()),
      congestion_manager_(clock_, kTCP),
      connected_(true),
      received_truncated_ack_(false),
      send_ack_in_response_to_packet_(false) {
  helper_->SetConnection(this);
  helper_->SetTimeoutAlarm(timeout_);
  framer_.set_visitor(this);
  framer_.set_entropy_calculator(&entropy_manager_);
  memset(&last_header_, 0, sizeof(last_header_));
  outgoing_ack_.sent_info.least_unacked = 0;
  outgoing_ack_.sent_info.entropy_hash = 0;
  outgoing_ack_.received_info.largest_observed = 0;
  outgoing_ack_.received_info.entropy_hash = 0;

  /*
  if (FLAGS_fake_packet_loss_percentage > 0) {
    int32 seed = RandomBase::WeakSeed32();
    LOG(INFO) << "Seeding packet loss with " << seed;
    random_.reset(new MTRandom(seed));
  }
  */
}

QuicConnection::~QuicConnection() {
  STLDeleteValues(&unacked_packets_);
  STLDeleteValues(&group_map_);
  for (QueuedPacketList::iterator it = queued_packets_.begin();
       it != queued_packets_.end(); ++it) {
    delete it->packet;
  }
}

void QuicConnection::OnError(QuicFramer* framer) {
  SendConnectionClose(framer->error());
}

void QuicConnection::OnPacket() {
  time_of_last_received_packet_ = clock_->Now();
  DVLOG(1) << "time of last received packet: "
           << time_of_last_received_packet_.ToMicroseconds();

  // TODO(alyssar, rch) handle migration!
  self_address_ = last_self_address_;
  peer_address_ = last_peer_address_;
}

void QuicConnection::OnPublicResetPacket(
    const QuicPublicResetPacket& packet) {
  if (debug_visitor_) {
    debug_visitor_->OnPublicResetPacket(packet);
  }
  CloseConnection(QUIC_PUBLIC_RESET, true);
}

void QuicConnection::OnRevivedPacket() {
}

bool QuicConnection::OnPacketHeader(const QuicPacketHeader& header) {
  if (debug_visitor_) {
    debug_visitor_->OnPacketHeader(header);
  }
  if (header.public_header.guid != guid_) {
    DLOG(INFO) << "Ignoring packet from unexpected GUID: "
               << header.public_header.guid << " instead of " << guid_;
    return false;
  }

  if (!Near(header.packet_sequence_number,
            last_header_.packet_sequence_number)) {
    DLOG(INFO) << "Packet " << header.packet_sequence_number
               << " out of bounds.  Discarding";
    // TODO(alyssar) close the connection entirely.
    return false;
  }

  // If this packet has already been seen, or that the sender
  // has told us will not be retransmitted, then stop processing the packet.
  if (!IsAwaitingPacket(outgoing_ack_.received_info,
                        header.packet_sequence_number)) {
    return false;
  }

  DVLOG(1) << "Received packet header: " << header;
  last_header_ = header;
  return true;
}

void QuicConnection::OnFecProtectedPayload(StringPiece payload) {
  DCHECK_NE(0u, last_header_.fec_group);
  QuicFecGroup* group = GetFecGroup();
  group->Update(last_header_, payload);
}

void QuicConnection::OnStreamFrame(const QuicStreamFrame& frame) {
  if (debug_visitor_) {
    debug_visitor_->OnStreamFrame(frame);
  }
  last_stream_frames_.push_back(frame);
}

void QuicConnection::OnAckFrame(const QuicAckFrame& incoming_ack) {
  if (debug_visitor_) {
    debug_visitor_->OnAckFrame(incoming_ack);
  }
  DVLOG(1) << "OnAckFrame: " << incoming_ack;

  if (last_header_.packet_sequence_number <= largest_seen_packet_with_ack_) {
    DLOG(INFO) << "Received an old ack frame: ignoring";
    return;
  }
  largest_seen_packet_with_ack_ = last_header_.packet_sequence_number;

  if (!ValidateAckFrame(incoming_ack)) {
    SendConnectionClose(QUIC_INVALID_ACK_DATA);
    return;
  }

  // TODO(satyamshekhar): Not true if missing_packets.size() was actually
  // kMaxUnackedPackets. This can result in a dead connection if all the
  // missing packets get lost during retransmission. Now the new packets(or the
  // older packets) will not be retransmitted due to RTO
  // since received_truncated_ack_ is true and their sequence_number is >
  // peer_largest_observed_packet. Fix either by resetting it in
  // MaybeRetransmitPacketForRTO or keeping an explicit flag for ack truncation.
  received_truncated_ack_ =
      incoming_ack.received_info.missing_packets.size() >= kMaxUnackedPackets;

  UpdatePacketInformationReceivedByPeer(incoming_ack);
  UpdatePacketInformationSentByPeer(incoming_ack);
  congestion_manager_.OnIncomingAckFrame(incoming_ack,
                                         time_of_last_received_packet_);

  // Now the we have received an ack, we might be able to send queued packets.
  if (queued_packets_.empty()) {
    return;
  }

  QuicTime::Delta delay = congestion_manager_.TimeUntilSend(
      time_of_last_received_packet_, false);
  if (delay.IsZero()) {
    helper_->UnregisterSendAlarmIfRegistered();
    if (!write_blocked_) {
      OnCanWrite();
    }
  } else {
    helper_->SetSendAlarm(delay);
  }
}

void QuicConnection::OnCongestionFeedbackFrame(
    const QuicCongestionFeedbackFrame& feedback) {
  if (debug_visitor_) {
    debug_visitor_->OnCongestionFeedbackFrame(feedback);
  }
  congestion_manager_.OnIncomingQuicCongestionFeedbackFrame(
      feedback, time_of_last_received_packet_);
}

bool QuicConnection::ValidateAckFrame(const QuicAckFrame& incoming_ack) {
  if (incoming_ack.received_info.largest_observed >
      packet_creator_.sequence_number()) {
    DLOG(ERROR) << "Client observed unsent packet:"
                << incoming_ack.received_info.largest_observed << " vs "
                << packet_creator_.sequence_number();
    // We got an error for data we have not sent.  Error out.
    return false;
  }

  if (incoming_ack.received_info.largest_observed <
      peer_largest_observed_packet_) {
    DLOG(ERROR) << "Client's largest_observed packet decreased:"
                << incoming_ack.received_info.largest_observed << " vs "
                << peer_largest_observed_packet_;
    // We got an error for data we have not sent.  Error out.
    return false;
  }

  // We can't have too many unacked packets, or our ack frames go over
  // kMaxPacketSize.
  DCHECK_LE(incoming_ack.received_info.missing_packets.size(),
            kMaxUnackedPackets);

  if (incoming_ack.sent_info.least_unacked < peer_least_packet_awaiting_ack_) {
    DLOG(ERROR) << "Client sent low least_unacked: "
                << incoming_ack.sent_info.least_unacked
                << " vs " << peer_least_packet_awaiting_ack_;
    // We never process old ack frames, so this number should only increase.
    return false;
  }

  if (incoming_ack.sent_info.least_unacked >
      last_header_.packet_sequence_number) {
    DLOG(ERROR) << "Client sent least_unacked:"
                << incoming_ack.sent_info.least_unacked
                << " greater than the enclosing packet sequence number:"
                << last_header_.packet_sequence_number;
    return false;
  }

  if (!incoming_ack.received_info.missing_packets.empty() &&
      *incoming_ack.received_info.missing_packets.rbegin() >
      incoming_ack.received_info.largest_observed) {
    DLOG(ERROR) << "Client sent missing packet: "
                << *incoming_ack.received_info.missing_packets.rbegin()
                << " greater than largest observed: "
                << incoming_ack.received_info.largest_observed;
    return false;
  }

  if (!incoming_ack.received_info.missing_packets.empty() &&
      *incoming_ack.received_info.missing_packets.begin() <
      least_packet_awaited_by_peer_) {
    DLOG(ERROR) << "Client sent missing packet: "
                << *incoming_ack.received_info.missing_packets.begin()
                << "smaller than least_packet_awaited_by_peer_: "
                << least_packet_awaited_by_peer_;
    return false;
  }

  if (!entropy_manager_.IsValidEntropy(
          incoming_ack.received_info.largest_observed,
          incoming_ack.received_info.missing_packets,
          incoming_ack.received_info.entropy_hash)) {
    DLOG(ERROR) << "Client sent invalid entropy.";
    return false;
  }

  return true;
}

void QuicConnection::UpdatePacketInformationReceivedByPeer(
    const QuicAckFrame& incoming_ack) {
  SequenceNumberSet acked_packets;

  // ValidateAck should fail if largest_observed ever shrinks.
  DCHECK_LE(peer_largest_observed_packet_,
            incoming_ack.received_info.largest_observed);
  peer_largest_observed_packet_ = incoming_ack.received_info.largest_observed;

  if (incoming_ack.received_info.missing_packets.empty()) {
    least_packet_awaited_by_peer_ = peer_largest_observed_packet_ + 1;
  } else {
    least_packet_awaited_by_peer_ =
        *(incoming_ack.received_info.missing_packets.begin());
  }

  entropy_manager_.ClearSentEntropyBefore(least_packet_awaited_by_peer_ - 1);

  int retransmitted_packets = 0;
  // Go through the packets we have not received an ack for and see if this
  // incoming_ack shows they've been seen by the peer.
  UnackedPacketMap::iterator it = unacked_packets_.begin();
  while (it != unacked_packets_.end()) {
    QuicPacketSequenceNumber sequence_number = it->first;
    if (sequence_number > peer_largest_observed_packet_) {
      break;
    }
    RetransmittableFrames* unacked = it->second;
    if (!IsAwaitingPacket(incoming_ack.received_info, sequence_number)) {
      // Packet was acked, so remove it from our unacked packet list.
      DVLOG(1) << "Got an ack for " << sequence_number;
      acked_packets.insert(sequence_number);
      delete unacked;
      UnackedPacketMap::iterator it_tmp = it;
      ++it;
      unacked_packets_.erase(it_tmp);
      retransmission_map_.erase(sequence_number);
    } else {
      // This is a packet which we planned on retransmitting and has not been
      // seen at the time of this ack being sent out.  See if it's our new
      // lowest unacked packet.
      DVLOG(1) << "still missing " << sequence_number;
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
        DVLOG(1) << "Trying to retransmit packet " << sequence_number
                 << " as it has been nacked 3 or more times.";
        // TODO(satyamshekhar): save in a vector and retransmit after the
        // loop.
        RetransmitPacket(sequence_number);
      }
    }
  }
  if (acked_packets.size() > 0) {
    visitor_->OnAck(acked_packets);
  }
}

bool QuicConnection::DontWaitForPacketsBefore(
    QuicPacketSequenceNumber least_unacked) {
  size_t missing_packets_count =
      outgoing_ack_.received_info.missing_packets.size();
   outgoing_ack_.received_info.missing_packets.erase(
      outgoing_ack_.received_info.missing_packets.begin(),
      outgoing_ack_.received_info.missing_packets.lower_bound(least_unacked));
   return missing_packets_count !=
       outgoing_ack_.received_info.missing_packets.size();
}

void QuicConnection::UpdatePacketInformationSentByPeer(
    const QuicAckFrame& incoming_ack) {
  // ValidateAck() should fail if peer_least_packet_awaiting_ack_ shrinks.
  DCHECK_LE(peer_least_packet_awaiting_ack_,
            incoming_ack.sent_info.least_unacked);
  if (incoming_ack.sent_info.least_unacked > peer_least_packet_awaiting_ack_) {
    bool missed_packets =
        DontWaitForPacketsBefore(incoming_ack.sent_info.least_unacked);
    if (missed_packets || incoming_ack.sent_info.least_unacked >
        outgoing_ack_.received_info.largest_observed + 1) {
      DVLOG(1) << "Updating entropy hashed since we missed packets";
      // There were some missing packets that we won't ever get now. Recalculate
      // the received entropy hash.
      entropy_manager_.RecalculateReceivedEntropyHash(
          incoming_ack.sent_info.least_unacked,
          incoming_ack.sent_info.entropy_hash);
    }
    peer_least_packet_awaiting_ack_ = incoming_ack.sent_info.least_unacked;
    // TODO(satyamshekhar): We get this iterator O(logN) in
    // RecalculateReceivedEntropyHash also.
    entropy_manager_.ClearReceivedEntropyBefore(
        peer_least_packet_awaiting_ack_);
  }
  DCHECK(outgoing_ack_.received_info.missing_packets.empty() ||
         *outgoing_ack_.received_info.missing_packets.begin() >=
             peer_least_packet_awaiting_ack_);
  // Possibly close any FecGroups which are now irrelevant
  CloseFecGroupsBefore(incoming_ack.sent_info.least_unacked + 1);
}

void QuicConnection::OnFecData(const QuicFecData& fec) {
  DCHECK_NE(0u, last_header_.fec_group);
  QuicFecGroup* group = GetFecGroup();
  group->UpdateFec(last_header_.packet_sequence_number,
                   last_header_.fec_entropy_flag, fec);
}

void QuicConnection::OnRstStreamFrame(const QuicRstStreamFrame& frame) {
  if (debug_visitor_) {
    debug_visitor_->OnRstStreamFrame(frame);
  }
  DLOG(INFO) << "Stream reset with error "
             << QuicUtils::ErrorToString(frame.error_code);
  visitor_->OnRstStream(frame);
}

void QuicConnection::OnConnectionCloseFrame(
    const QuicConnectionCloseFrame& frame) {
  if (debug_visitor_) {
    debug_visitor_->OnConnectionCloseFrame(frame);
  }
  DLOG(INFO) << "Connection closed with error "
             << QuicUtils::ErrorToString(frame.error_code);
  CloseConnection(frame.error_code, true);
}

void QuicConnection::OnGoAwayFrame(const QuicGoAwayFrame& frame) {
  DLOG(INFO) << "Go away received with error "
             << QuicUtils::ErrorToString(frame.error_code)
             << " and reason:" << frame.reason_phrase;
  visitor_->OnGoAway(frame);
}

void QuicConnection::OnPacketComplete() {
  // TODO(satyamshekhar): Don't do anything if this packet closed the
  // connection.
  if (!last_packet_revived_) {
    DLOG(INFO) << "Got packet " << last_header_.packet_sequence_number
               << " with " << last_stream_frames_.size()
               << " stream frames for " << last_header_.public_header.guid;
    congestion_manager_.RecordIncomingPacket(
        last_size_, last_header_.packet_sequence_number,
        time_of_last_received_packet_, last_packet_revived_);
  } else {
    DLOG(INFO) << "Got revived packet with " << last_stream_frames_.size()
               << " frames.";
  }

  if ((last_stream_frames_.empty() ||
       visitor_->OnPacket(self_address_, peer_address_,
                          last_header_, last_stream_frames_))) {
    RecordPacketReceived(last_header_);
  }

  MaybeSendAckInResponseToPacket();
  last_stream_frames_.clear();
}

QuicAckFrame* QuicConnection::CreateAckFrame() {
  return new QuicAckFrame(outgoing_ack_);
}

QuicCongestionFeedbackFrame* QuicConnection::CreateFeedbackFrame() {
  return new QuicCongestionFeedbackFrame(outgoing_congestion_feedback_);
}

void QuicConnection::MaybeSendAckInResponseToPacket() {
  if (send_ack_in_response_to_packet_) {
    SendAck();
  } else if (!last_stream_frames_.empty()) {
    // TODO(alyssar) this case should really be "if the packet contained any
    // non-ack frame", rather than "if the packet contained a stream frame"
    helper_->SetAckAlarm(congestion_manager_.DefaultRetransmissionTime());
  }
  send_ack_in_response_to_packet_ = !send_ack_in_response_to_packet_;
}

QuicConsumedData QuicConnection::SendStreamData(QuicStreamId id,
                                                base::StringPiece data,
                                                QuicStreamOffset offset,
                                                bool fin) {
  return packet_generator_.ConsumeData(id, data, offset, fin);
}

void QuicConnection::SendRstStream(QuicStreamId id,
                                   QuicErrorCode error) {
  packet_generator_.AddControlFrame(
      QuicFrame(new QuicRstStreamFrame(id, error)));
}

void QuicConnection::ProcessUdpPacket(const IPEndPoint& self_address,
                                      const IPEndPoint& peer_address,
                                      const QuicEncryptedPacket& packet) {
  if (debug_visitor_) {
    debug_visitor_->OnPacketReceived(self_address, peer_address, packet);
  }
  last_packet_revived_ = false;
  last_size_ = packet.length();
  last_self_address_ = self_address;
  last_peer_address_ = peer_address;
  framer_.ProcessPacket(packet);
  MaybeProcessRevivedPacket();
}

bool QuicConnection::OnCanWrite() {
  LOG(INFO) << "here!!!";
  write_blocked_ = false;

  WriteQueuedPackets();

  // Sending queued packets may have caused the socket to become write blocked,
  // or the congestion manager to prohibit sending.  If we've sent everything
  // we had queued and we're still not blocked, let the visitor know it can
  // write more.
  // TODO(rch): shouldn't this be "if (CanWrite(false))"
  if (!write_blocked_) {
    packet_generator_.StartBatchOperations();
    bool all_bytes_written = visitor_->OnCanWrite();
    packet_generator_.FinishBatchOperations();

    // After the visitor writes, it may have caused the socket to become write
    // blocked or the congestion manager to prohibit sending, so check again.
    if (!write_blocked_ && !all_bytes_written && !helper_->IsSendAlarmSet()) {
      // We're not write blocked, but some stream didn't write out all of its
      // bytes.  Register for 'immediate' resumption so we'll keep writing after
      // other quic connections have had a chance to use the socket.
      helper_->SetSendAlarm(QuicTime::Delta::Zero());
    }
  }

  return !write_blocked_;
}

bool QuicConnection::WriteQueuedPackets() {
  DCHECK(!write_blocked_);

  size_t num_queued_packets = queued_packets_.size() + 1;
  QueuedPacketList::iterator packet_iterator = queued_packets_.begin();
  while (!write_blocked_ && !helper_->IsSendAlarmSet() &&
         packet_iterator != queued_packets_.end()) {
    // Ensure that from one iteration of this loop to the next we
    // succeeded in sending a packet so we don't infinitely loop.
    // TODO(rch): clean up and close the connection if we really hit this.
    DCHECK_LT(queued_packets_.size(), num_queued_packets);
    num_queued_packets = queued_packets_.size();
    if (WritePacket(packet_iterator->sequence_number,
                    packet_iterator->packet, !kForce)) {
      packet_iterator = queued_packets_.erase(packet_iterator);
    } else {
      // Continue, because some queued packets may still be writable.
      ++packet_iterator;
    }
  }

  return !write_blocked_;
}

void QuicConnection::RecordPacketReceived(const QuicPacketHeader& header) {
  DLOG(INFO) << "Recording received packet: " << header.packet_sequence_number;
  QuicPacketSequenceNumber sequence_number = header.packet_sequence_number;
  DCHECK(IsAwaitingPacket(outgoing_ack_.received_info, sequence_number));

  InsertMissingPacketsBetween(
      &outgoing_ack_.received_info,
      max(outgoing_ack_.received_info.largest_observed + 1,
          peer_least_packet_awaiting_ack_),
      header.packet_sequence_number);

  if (outgoing_ack_.received_info.largest_observed >
      header.packet_sequence_number) {
    // We've gotten one of the out of order packets - remove it from our
    // "missing packets" list.
    DVLOG(1) << "Removing "  << sequence_number << " from missing list";
    outgoing_ack_.received_info.missing_packets.erase(sequence_number);
  }
  outgoing_ack_.received_info.largest_observed = max(
      outgoing_ack_.received_info.largest_observed,
      header.packet_sequence_number);
  entropy_manager_.RecordReceivedPacketEntropyHash(sequence_number,
                                                   header.entropy_hash);
}

bool QuicConnection::MaybeRetransmitPacketForRTO(
    QuicPacketSequenceNumber sequence_number) {
  DCHECK_EQ(ContainsKey(unacked_packets_, sequence_number),
            ContainsKey(retransmission_map_, sequence_number));

  if (!ContainsKey(unacked_packets_, sequence_number)) {
    DVLOG(2) << "alarm fired for " << sequence_number
             << " but it has been acked or already retransmitted with"
             << " different sequence number.";
    // So no extra delay is added for this packet.
    return true;
  }

  // If the packet hasn't been acked and we're getting truncated acks, ignore
  // any RTO for packets larger than the peer's largest observed packet; it may
  // have been received by the peer and just wasn't acked due to the ack frame
  // running out of space.
  if (received_truncated_ack_ &&
      sequence_number > peer_largest_observed_packet_) {
    return false;
  } else {
    RetransmitPacket(sequence_number);
    return true;
  }
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
  // TODO(ianswett): Never change the sequence number of the connect packet.
  // Re-packetize the frames with a new sequence number for retransmission.
  // Retransmitted data packets do not use FEC, even when it's enabled.
  SerializedPacket serialized_packet =
      packet_creator_.SerializeAllFrames(unacked->frames());
  RetransmissionInfo retransmission_info(serialized_packet.sequence_number);
  retransmission_info.number_retransmissions =
      retransmission_it->second.number_retransmissions + 1;
  retransmission_map_.insert(make_pair(serialized_packet.sequence_number,
                                       retransmission_info));
  // Remove info with old sequence number.
  unacked_packets_.erase(unacked_it);
  retransmission_map_.erase(retransmission_it);
  DVLOG(1) << "Retransmitting unacked packet " << sequence_number << " as "
           << serialized_packet.sequence_number;
  DCHECK(unacked_packets_.empty() ||
         unacked_packets_.rbegin()->first < serialized_packet.sequence_number);
  unacked_packets_.insert(make_pair(serialized_packet.sequence_number,
                                    unacked));
  SendOrQueuePacket(serialized_packet.sequence_number,
                    serialized_packet.packet,
                    serialized_packet.entropy_hash);
}

bool QuicConnection::CanWrite(bool is_retransmission) {
  // TODO(ianswett): If the packet is a retransmit, the current send alarm may
  // be too long.
  if (write_blocked_ || helper_->IsSendAlarmSet()) {
    return false;
  }
  QuicTime::Delta delay = congestion_manager_.TimeUntilSend(clock_->Now(),
                                                            is_retransmission);
  // If the scheduler requires a delay, then we can not send this packet now.
  if (!delay.IsZero() && !delay.IsInfinite()) {
    // TODO(pwestin): we need to handle delay.IsInfinite() separately.
    helper_->SetSendAlarm(delay);
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

void QuicConnection::MaybeSetupRetransmission(
    QuicPacketSequenceNumber sequence_number) {
  RetransmissionMap::iterator it = retransmission_map_.find(sequence_number);
  if (it == retransmission_map_.end()) {
    DVLOG(1) << "Will not retransmit packet " << sequence_number;
    return;
  }

  RetransmissionInfo retransmission_info = it->second;
  QuicTime::Delta retransmission_delay =
      congestion_manager_.GetRetransmissionDelay(
          unacked_packets_.size(),
          retransmission_info.number_retransmissions);
  retransmission_info.scheduled_time =
      clock_->ApproximateNow().Add(retransmission_delay);
  retransmission_timeouts_.push(retransmission_info);

  // Do not set the retransmisson alarm if we're already handling the
  // retransmission alarm because the retransmission alarm will be reset when
  // OnRetransmissionTimeout completes.
  if (!handling_retransmission_timeout_) {
    helper_->SetRetransmissionAlarm(retransmission_delay);
  }
  // TODO(satyamshekhar): restore pacekt reordering with Ian's TODO in
  // SendStreamData().
}

bool QuicConnection::WritePacket(QuicPacketSequenceNumber sequence_number,
                                 QuicPacket* packet,
                                 bool forced) {
  if (!connected_) {
    DLOG(INFO)
        << "Dropping packet to be sent since connection is disconnected.";
    delete packet;
    // Returning true because we deleted the packet and the caller shouldn't
    // delete it again.
    return true;
  }

  bool is_retransmission = IsRetransmission(sequence_number);
  // If we are not forced and we can't write, then simply return false;
  if (!forced && !CanWrite(is_retransmission)) {
    return false;
  }

  scoped_ptr<QuicEncryptedPacket> encrypted(
      framer_.EncryptPacket(sequence_number, *packet));
  DLOG(INFO) << "Sending packet number " << sequence_number << " : "
             << (packet->is_fec_packet() ? "FEC " :
                 (ContainsKey(retransmission_map_, sequence_number) ?
                  "data bearing " : " ack only "));

  DCHECK(encrypted->length() <= kMaxPacketSize)
      << "Packet " << sequence_number << " will not be read; too large: "
      << packet->length() << " " << encrypted->length() << " "
      << outgoing_ack_;

  int error;
  QuicTime now = clock_->Now();
  int rv = helper_->WritePacketToWire(*encrypted, &error);
  if (rv == -1 && error == ERR_IO_PENDING) {
    // TODO(satyashekhar): It might be more efficient (fewer system calls), if
    // all connections share this variable i.e this becomes a part of
    // PacketWriterInterface.
    write_blocked_ = true;
    return false;
  }
  time_of_last_sent_packet_ = now;
  DVLOG(1) << "time of last sent packet: " << now.ToMicroseconds();
  // TODO(wtc): Is it correct to continue if the write failed.

  // Set the retransmit alarm only when we have sent the packet to the client
  // and not when it goes to the pending queue, otherwise we will end up adding
  // an entry to retransmission_timeout_ every time we attempt a write.
  MaybeSetupRetransmission(sequence_number);

  congestion_manager_.SentPacket(sequence_number, now, packet->length(),
                                 is_retransmission);
  delete packet;
  return true;
}

bool QuicConnection::OnSerializedPacket(
    const SerializedPacket& serialized_packet) {
  if (serialized_packet.retransmittable_frames != NULL) {
    DCHECK(unacked_packets_.empty() ||
           unacked_packets_.rbegin()->first <
               serialized_packet.sequence_number);
    unacked_packets_.insert(
      make_pair(serialized_packet.sequence_number,
                serialized_packet.retransmittable_frames));
    // All unacked packets might be retransmitted.
    retransmission_map_.insert(
        make_pair(serialized_packet.sequence_number,
                  RetransmissionInfo(serialized_packet.sequence_number)));
  }
  return SendOrQueuePacket(serialized_packet.sequence_number,
                    serialized_packet.packet,
                    serialized_packet.entropy_hash);
}

bool QuicConnection::SendOrQueuePacket(QuicPacketSequenceNumber sequence_number,
                                       QuicPacket* packet,
                                       QuicPacketEntropyHash entropy_hash) {
  entropy_manager_.RecordSentPacketEntropyHash(sequence_number, entropy_hash);
  if (!WritePacket(sequence_number, packet, !kForce)) {
    queued_packets_.push_back(QueuedPacket(sequence_number, packet));
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

void QuicConnection::UpdateOutgoingAck() {
  if (!unacked_packets_.empty()) {
    outgoing_ack_.sent_info.least_unacked = unacked_packets_.begin()->first;
  } else {
    // If there are no unacked packets, set the least unacked packet to
    // sequence_number() + 1 since that will be the sequence number of this
    // ack packet whenever it is sent.
    outgoing_ack_.sent_info.least_unacked =
        packet_creator_.sequence_number() + 1;
  }
  outgoing_ack_.sent_info.entropy_hash = entropy_manager_.SentEntropyHash(
      outgoing_ack_.sent_info.least_unacked - 1);
  outgoing_ack_.received_info.entropy_hash =
      entropy_manager_.ReceivedEntropyHash(
          outgoing_ack_.received_info.largest_observed);
}

void QuicConnection::SendAck() {
  helper_->ClearAckAlarm();
  UpdateOutgoingAck();
  DVLOG(1) << "Sending ack: " << outgoing_ack_;

  // TODO(rch): delay this until the CreateFeedbackFrame
  // method is invoked.  This requires changes SetShouldSendAck
  // to be a no-arg method, and re-jiggering its implementation.
  bool send_feedback = false;
  if (congestion_manager_.GenerateCongestionFeedback(
          &outgoing_congestion_feedback_)) {
    DVLOG(1) << "Sending feedback " << outgoing_congestion_feedback_;
    send_feedback = true;
  }

  packet_generator_.SetShouldSendAck(send_feedback);
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

  for (int i = 0; i < kMaxPacketsPerRetransmissionAlarm &&
           !retransmission_timeouts_.empty(); ++i) {
    RetransmissionInfo retransmission_info = retransmission_timeouts_.top();
    DCHECK(retransmission_info.scheduled_time.IsInitialized());
    if (retransmission_info.scheduled_time > clock_->ApproximateNow()) {
      break;
    }
    retransmission_timeouts_.pop();
    if (!MaybeRetransmitPacketForRTO(retransmission_info.sequence_number)) {
      DLOG(INFO) << "MaybeRetransmitPacketForRTO failed: "
                 << "adding an extra delay for "
                 << retransmission_info.sequence_number;
      retransmission_info.scheduled_time = clock_->ApproximateNow().Add(
          congestion_manager_.DefaultRetransmissionTime());
      retransmission_timeouts_.push(retransmission_info);
    }
  }

  handling_retransmission_timeout_ = false;

  if (retransmission_timeouts_.empty()) {
    return QuicTime::FromMilliseconds(0);
  }

  // We have packets remaining.  Return the absolute RTO of the oldest packet
  // on the list.
  return retransmission_timeouts_.top().scheduled_time;
}

void QuicConnection::MaybeProcessRevivedPacket() {
  QuicFecGroup* group = GetFecGroup();
  if (group == NULL || !group->CanRevive()) {
    return;
  }
  QuicPacketHeader revived_header;
  char revived_payload[kMaxPacketSize];
  size_t len = group->Revive(&revived_header, revived_payload, kMaxPacketSize);
  revived_header.public_header.guid = guid_;
  revived_header.public_header.version_flag = false;
  revived_header.public_header.reset_flag = false;
  revived_header.fec_flag = false;
  revived_header.fec_group = kNoFecOffset;
  group_map_.erase(last_header_.fec_group);
  delete group;

  last_packet_revived_ = true;
  if (debug_visitor_) {
    debug_visitor_->OnRevivedPacket(revived_header,
                                    StringPiece(revived_payload, len));
  }
  framer_.ProcessRevivedPacket(&revived_header,
                               StringPiece(revived_payload, len));
}

QuicFecGroup* QuicConnection::GetFecGroup() {
  QuicFecGroupNumber fec_group_num = last_header_.fec_group;
  if (fec_group_num == 0) {
    return NULL;
  }
  if (group_map_.count(fec_group_num) == 0) {
    // TODO(rch): limit the number of active FEC groups.
    group_map_[fec_group_num] = new QuicFecGroup();
  }
  return group_map_[fec_group_num];
}

void QuicConnection::SendConnectionClose(QuicErrorCode error) {
  SendConnectionCloseWithDetails(error, string());
}

void QuicConnection::SendConnectionClosePacket(QuicErrorCode error,
                                               const string& details) {
  DLOG(INFO) << "Force closing with error " << QuicUtils::ErrorToString(error)
             << " (" << error << ")";
  QuicConnectionCloseFrame frame;
  frame.error_code = error;
  frame.error_details = details;
  UpdateOutgoingAck();
  frame.ack_frame = outgoing_ack_;

  SerializedPacket serialized_packet =
      packet_creator_.SerializeConnectionClose(&frame);
  SendOrQueuePacket(serialized_packet.sequence_number, serialized_packet.packet,
                    serialized_packet.entropy_hash);
}

void QuicConnection::SendConnectionCloseWithDetails(QuicErrorCode error,
                                                    const string& details) {
  SendConnectionClosePacket(error, details);
  CloseConnection(error, false);
}

void QuicConnection::CloseConnection(QuicErrorCode error, bool from_peer) {
  // TODO(satyamshekhar): Ask the dispatcher to delete visitor and hence self
  // if the visitor will always be deleted by closing the connection.
  connected_ = false;
  visitor_->ConnectionClose(error, from_peer);
}

void QuicConnection::SendGoAway(QuicErrorCode error,
                                QuicStreamId last_good_stream_id,
                                const string& reason) {
  DLOG(INFO) << "Going away with error " << QuicUtils::ErrorToString(error)
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
  return !queued_packets_.empty() || packet_generator_.HasQueuedData();
}

bool QuicConnection::CheckForTimeout() {
  QuicTime now = clock_->ApproximateNow();
  QuicTime time_of_last_packet = std::max(time_of_last_received_packet_,
                                          time_of_last_sent_packet_);

  QuicTime::Delta delta = now.Subtract(time_of_last_packet);
  DVLOG(1) << "last packet " << time_of_last_packet.ToMicroseconds()
           << " now:" << now.ToMicroseconds()
           << " delta:" << delta.ToMicroseconds();
  if (delta >= timeout_) {
    SendConnectionClose(QUIC_CONNECTION_TIMED_OUT);
    return true;
  }
  helper_->SetTimeoutAlarm(timeout_.Subtract(delta));
  return false;
}

}  // namespace net
