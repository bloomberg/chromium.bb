// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_connection.h"

#include <algorithm>

#include "base/logging.h"
#include "base/stl_util.h"
#include "net/base/net_errors.h"
#include "net/quic/congestion_control/quic_receipt_metrics_collector.h"
#include "net/quic/congestion_control/quic_send_scheduler.h"
#include "net/quic/quic_utils.h"

using base::hash_map;
using base::hash_set;
using base::StringPiece;
using std::list;
using std::make_pair;
using std::min;
using std::vector;
using std::set;

namespace net {

// TODO(pwestin): kDefaultTimeoutUs is in int64.
int32 kNegotiatedTimeoutUs = kDefaultTimeoutUs;

// The largest gap in packets we'll accept without closing the connection.
// This will likely have to be tuned.
const QuicPacketSequenceNumber kMaxPacketGap = 5000;

// The maximum number of nacks which can be transmitted in a single ack packet
// without exceeding kMaxPacketSize.
const QuicPacketSequenceNumber kMaxUnackedPackets = 192u;

// The amount of time we wait before resending a packet.
const int64 kDefaultResendTimeMs = 500;

// We want to make sure if we get a large nack packet, we don't queue up too
// many packets at once.  10 is arbitrary.
const int kMaxResendPerAck = 10;

// TCP resends after 2 nacks.  We allow for a third in case of out-of-order
// delivery.
// TODO(ianswett): Change to match TCP's rule of resending once an ack at least
// 3 sequence numbers larger arrives.
const int kNumberOfNacksBeforeResend = 3;

// The maxiumum number of packets we'd like to queue.  We may end up queueing
// more in the case of many control frames.
// 6 is arbitrary.
const int kMaxPacketsToSerializeAtOnce = 6;

bool Near(QuicPacketSequenceNumber a, QuicPacketSequenceNumber b) {
  QuicPacketSequenceNumber delta = (a > b) ? a - b : b - a;
  return delta <= kMaxPacketGap;
}

QuicConnection::UnackedPacket::UnackedPacket(QuicFrames unacked_frames)
    : frames(unacked_frames),
      number_nacks(0) {
}

QuicConnection::UnackedPacket::UnackedPacket(QuicFrames unacked_frames,
                                             std::string data)
    : frames(unacked_frames),
      number_nacks(0),
      data(data) {
}

QuicConnection::UnackedPacket::~UnackedPacket() {
}

QuicConnection::QuicConnection(QuicGuid guid,
                               IPEndPoint address,
                               QuicConnectionHelperInterface* helper)
    : helper_(helper),
      framer_(QuicDecrypter::Create(kNULL), QuicEncrypter::Create(kNULL)),
      clock_(helper->GetClock()),
      random_generator_(helper->GetRandomGenerator()),
      guid_(guid),
      peer_address_(address),
      should_send_ack_(false),
      should_send_congestion_feedback_(false),
      largest_seen_packet_with_ack_(0),
      peer_largest_observed_packet_(0),
      peer_least_packet_awaiting_ack_(0),
      write_blocked_(false),
      packet_creator_(guid_, &framer_),
      timeout_(QuicTime::Delta::FromMicroseconds(kDefaultTimeoutUs)),
      time_of_last_packet_(clock_->Now()),
      collector_(new QuicReceiptMetricsCollector(clock_, kTCP)),
      scheduler_(new QuicSendScheduler(clock_, kTCP)),
      connected_(true),
      received_truncated_ack_(false),
      send_ack_in_response_to_packet_(false) {
  options()->max_num_packets = kMaxPacketsToSerializeAtOnce;
  helper_->SetConnection(this);
  helper_->SetTimeoutAlarm(timeout_);
  framer_.set_visitor(this);
  memset(&last_header_, 0, sizeof(last_header_));
  outgoing_ack_.sent_info.least_unacked = 0;
  outgoing_ack_.received_info.largest_observed = 0;

  /*
  if (FLAGS_fake_packet_loss_percentage > 0) {
    int32 seed = RandomBase::WeakSeed32();
    LOG(INFO) << "Seeding packet loss with " << seed;
    random_.reset(new MTRandom(seed));
  }
  */
}

QuicConnection::~QuicConnection() {
  for (UnackedPacketMap::iterator u = unacked_packets_.begin();
       u != unacked_packets_.end(); ++u) {
    DeleteUnackedPacket(u->second);
  }
  STLDeleteValues(&unacked_packets_);
  STLDeleteValues(&group_map_);
  for (QueuedPacketList::iterator q = queued_packets_.begin();
       q != queued_packets_.end(); ++q) {
    delete q->packet;
  }
}

void QuicConnection::DeleteEnclosedFrame(QuicFrame* frame) {
  switch (frame->type) {
    case STREAM_FRAME:
      delete frame->stream_frame;
      break;
    case ACK_FRAME:
      delete frame->ack_frame;
      break;
    case CONGESTION_FEEDBACK_FRAME:
      delete frame->congestion_feedback_frame;
      break;
    case RST_STREAM_FRAME:
      delete frame->rst_stream_frame;
      break;
    case CONNECTION_CLOSE_FRAME:
      delete frame->connection_close_frame;
      break;
    case PDU_FRAME:  // Fall through.
    case NUM_FRAME_TYPES:
      DCHECK(false) << "Cannot delete type: " << frame->type;
  }
}

void QuicConnection::DeleteUnackedPacket(UnackedPacket* unacked) {
  for (QuicFrames::iterator it = unacked->frames.begin();
       it != unacked->frames.end(); ++it) {
    DCHECK(ShouldResend(*it));
    DeleteEnclosedFrame(&(*it));
  }
}

bool QuicConnection::ShouldResend(const QuicFrame& frame) {
  return frame.type != ACK_FRAME && frame.type != CONGESTION_FEEDBACK_FRAME;
}

void QuicConnection::OnError(QuicFramer* framer) {
  SendConnectionClose(framer->error());
}

void QuicConnection::OnPacket(const IPEndPoint& self_address,
                              const IPEndPoint& peer_address) {
  time_of_last_packet_ = clock_->Now();
  DVLOG(1) << "last packet: " << time_of_last_packet_.ToMicroseconds();

  // TODO(alyssar, rch) handle migration!
  self_address_ = self_address;
  peer_address_ = peer_address;
}

void QuicConnection::OnRevivedPacket() {
}

bool QuicConnection::OnPacketHeader(const QuicPacketHeader& header) {
  if (!Near(header.packet_sequence_number,
            last_header_.packet_sequence_number)) {
    DLOG(INFO) << "Packet " << header.packet_sequence_number
               << " out of bounds.  Discarding";
    // TODO(alyssar) close the connection entirely.
    return false;
  }

  // If this packet has already been seen, or that the sender
  // has told us will not be resent, then stop processing the packet.
  if (!outgoing_ack_.received_info.IsAwaitingPacket(
          header.packet_sequence_number)) {
    return false;
  }

  last_header_ = header;
  return true;
}

void QuicConnection::OnFecProtectedPayload(StringPiece payload) {
  DCHECK_NE(0, last_header_.fec_group);
  QuicFecGroup* group = GetFecGroup();
  group->Update(last_header_, payload);
}

void QuicConnection::OnStreamFrame(const QuicStreamFrame& frame) {
  last_stream_frames_.push_back(frame);
}

void QuicConnection::OnAckFrame(const QuicAckFrame& incoming_ack) {
  DVLOG(1) << "Ack packet: " << incoming_ack;

  if (last_header_.packet_sequence_number <= largest_seen_packet_with_ack_) {
    DLOG(INFO) << "Received an old ack frame: ignoring";
    return;
  }
  largest_seen_packet_with_ack_ = last_header_.packet_sequence_number;

  if (!ValidateAckFrame(incoming_ack)) {
    SendConnectionClose(QUIC_INVALID_ACK_DATA);
    return;
  }

  received_truncated_ack_ =
      incoming_ack.received_info.missing_packets.size() >= kMaxUnackedPackets;

  UpdatePacketInformationReceivedByPeer(incoming_ack);
  UpdatePacketInformationSentByPeer(incoming_ack);
  scheduler_->OnIncomingAckFrame(incoming_ack);

  // Now the we have received an ack, we might be able to send queued packets.
  if (queued_packets_.empty()) {
    return;
  }

  QuicTime::Delta delay = scheduler_->TimeUntilSend(false);
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
  scheduler_->OnIncomingQuicCongestionFeedbackFrame(feedback);
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
    DLOG(INFO) << "Client sent low least_unacked: "
               << incoming_ack.sent_info.least_unacked
               << " vs " << peer_least_packet_awaiting_ack_;
    // We never process old ack frames, so this number should only increase.
    return false;
  }

  if (incoming_ack.sent_info.least_unacked >
      last_header_.packet_sequence_number) {
    DLOG(INFO) << "Client sent least_unacked:"
               << incoming_ack.sent_info.least_unacked
               << " greater than the enclosing packet sequence number:"
               << last_header_.packet_sequence_number;
    return false;
  }

  return true;
}

void QuicConnection::UpdatePacketInformationReceivedByPeer(
    const QuicAckFrame& incoming_ack) {
  QuicConnectionVisitorInterface::AckedPackets acked_packets;

  // ValidateAck should fail if largest_observed ever shrinks.
  DCHECK_LE(peer_largest_observed_packet_,
            incoming_ack.received_info.largest_observed);
  peer_largest_observed_packet_ = incoming_ack.received_info.largest_observed;

  // Pick an upper bound for the lowest_unacked; we'll then loop through the
  // unacked packets and lower it if necessary.
  QuicPacketSequenceNumber lowest_unacked = min(
      packet_creator_.sequence_number() + 1,
      peer_largest_observed_packet_ + 1);

  int resent_packets = 0;

  // Go through the packets we have not received an ack for and see if this
  // incoming_ack shows they've been seen by the peer.
  UnackedPacketMap::iterator it = unacked_packets_.begin();
  while (it != unacked_packets_.end()) {
    if (!incoming_ack.received_info.IsAwaitingPacket(it->first)) {
      // Packet was acked, so remove it from our unacked packet list.
      DVLOG(1) << "Got an ack for " << it->first;
      // TODO(rch): This is inefficient and should be sped up.
      // TODO(ianswett): Ensure this inner loop is applicable now that we're
      // always sending packets with new sequence numbers.  I believe it may
      // only be relevant for the first crypto connect packet, which doesn't
      // get a new packet sequence number.
      // The acked packet might be queued (if a resend had been attempted).
      for (QueuedPacketList::iterator q = queued_packets_.begin();
           q != queued_packets_.end(); ++q) {
        if (q->sequence_number == it->first) {
          queued_packets_.erase(q);
          break;
        }
      }
      acked_packets.insert(it->first);
      DeleteUnackedPacket(it->second);
      delete it->second;
      UnackedPacketMap::iterator it_tmp = it;
      ++it;
      unacked_packets_.erase(it_tmp);
    } else {
      // This is a packet which we planned on resending and has not been
      // seen at the time of this ack being sent out.  See if it's our new
      // lowest unacked packet.
      DVLOG(1) << "still missing " << it->first;
      if (it->first < lowest_unacked) {
        lowest_unacked = it->first;
      }

      // Determine if this packet is being explicitly nacked and, if so, if it
      // is worth resending.
      QuicPacketSequenceNumber resend_number = 0;
      if (it->first < peer_largest_observed_packet_) {
        // The peer got packets after this sequence number.  This is an explicit
        // nack.
        ++(it->second->number_nacks);
        if (it->second->number_nacks >= kNumberOfNacksBeforeResend &&
            resent_packets < kMaxResendPerAck) {
          resend_number = it->first;
        }
      }

      ++it;
      if (resend_number > 0) {
        ++resent_packets;
        DVLOG(1) << "Trying to resend packet " << resend_number
                 << " as it has been nacked 3 or more times.";
        MaybeResendPacket(resend_number);
      }
    }
  }
  if (acked_packets.size() > 0) {
    visitor_->OnAck(acked_packets);
  }
  SetLeastUnacked(lowest_unacked);
}

void QuicConnection::SetLeastUnacked(QuicPacketSequenceNumber lowest_unacked) {
  // If we've gotten an ack for the lowest packet we were waiting on,
  // update that and the list of packets we advertise we will not resend.
  if (lowest_unacked > outgoing_ack_.sent_info.least_unacked) {
    outgoing_ack_.sent_info.least_unacked = lowest_unacked;
  }
}

void QuicConnection::UpdateLeastUnacked(
    QuicPacketSequenceNumber acked_sequence_number) {
  if (acked_sequence_number != outgoing_ack_.sent_info.least_unacked) {
    return;
  }
  QuicPacketSequenceNumber least_unacked =
      packet_creator_.sequence_number() + 1;
  for (UnackedPacketMap::iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it) {
    least_unacked = min<int>(least_unacked, it->first);
  }

  SetLeastUnacked(least_unacked);
}

void QuicConnection::UpdatePacketInformationSentByPeer(
    const QuicAckFrame& incoming_ack) {
  // Make sure we also don't ack any packets lower than the peer's
  // last-packet-awaiting-ack.
  if (incoming_ack.sent_info.least_unacked > peer_least_packet_awaiting_ack_) {
    outgoing_ack_.received_info.ClearMissingBefore(
        incoming_ack.sent_info.least_unacked);
    peer_least_packet_awaiting_ack_ = incoming_ack.sent_info.least_unacked;
  }

  // Possibly close any FecGroups which are now irrelevant
  CloseFecGroupsBefore(incoming_ack.sent_info.least_unacked + 1);
}

void QuicConnection::OnFecData(const QuicFecData& fec) {
  DCHECK_NE(0, last_header_.fec_group);
  QuicFecGroup* group = GetFecGroup();
  group->UpdateFec(last_header_.packet_sequence_number, fec);
}

void QuicConnection::OnRstStreamFrame(const QuicRstStreamFrame& frame) {
  DLOG(INFO) << "Stream reset with error "
             << QuicUtils::ErrorToString(frame.error_code);
  visitor_->OnRstStream(frame);
}

void QuicConnection::OnConnectionCloseFrame(
    const QuicConnectionCloseFrame& frame) {
  DLOG(INFO) << "Connection closed with error "
             << QuicUtils::ErrorToString(frame.error_code);
  connected_ = false;
  visitor_->ConnectionClose(frame.error_code, true);
}

void QuicConnection::OnPacketComplete() {
  if (!last_packet_revived_) {
    DLOG(INFO) << "Got packet " << last_header_.packet_sequence_number
               << " with " << last_stream_frames_.size()
               << " stream frames for " << last_header_.guid;
    collector_->RecordIncomingPacket(last_size_,
                                     last_header_.packet_sequence_number,
                                     clock_->Now(),
                                     last_packet_revived_);
  } else {
    DLOG(INFO) << "Got revived packet with " << last_stream_frames_.size()
               << " frames.";
  }

  if (last_stream_frames_.empty() ||
      visitor_->OnPacket(self_address_, peer_address_,
                         last_header_, last_stream_frames_)) {
    RecordPacketReceived(last_header_);
  }

  MaybeSendAckInResponseToPacket();
  last_stream_frames_.clear();
}

void QuicConnection::MaybeSendAckInResponseToPacket() {
  if (send_ack_in_response_to_packet_) {
    SendAck();
  } else if (!last_stream_frames_.empty()) {
    // TODO(alyssar) this case should really be "if the packet contained any
    // non-ack frame", rather than "if the packet contained a stream frame"
    helper_->SetAckAlarm(
        QuicTime::Delta::FromMicroseconds(kDefaultResendTimeMs));
  }
  send_ack_in_response_to_packet_ = !send_ack_in_response_to_packet_;
}

QuicConsumedData QuicConnection::SendStreamData(
    QuicStreamId id,
    StringPiece data,
    QuicStreamOffset offset,
    bool fin,
    QuicPacketSequenceNumber* last_packet) {
  size_t total_bytes_consumed = 0;
  bool fin_consumed = false;

  while (queued_packets_.empty()) {
    vector<PacketPair> packets;
    QuicFrames frames;
    size_t bytes_consumed =
        packet_creator_.DataToStream(id, data, offset, fin, &packets, &frames);
    total_bytes_consumed += bytes_consumed;
    offset += bytes_consumed;
    fin_consumed = fin && bytes_consumed == data.size();
    data.remove_prefix(bytes_consumed);
    DCHECK_LT(0u, packets.size());

    for (size_t i = 0; i < packets.size(); ++i) {
      // Resend is false for FEC packets.
      bool should_resend = !packets[i].second->IsFecPacket();
      SendPacket(packets[i].first,
                 packets[i].second,
                 should_resend,
                 false,
                 false);
      if (should_resend) {
        QuicFrames unacked_frames;
        unacked_frames.push_back(frames[i]);
        UnackedPacket* unacked = new UnackedPacket(
            unacked_frames, frames[i].stream_frame->data.as_string());
        // Ensure the string piece points to the owned copy of the data.
        unacked->frames[0].stream_frame->data = StringPiece(unacked->data);
        unacked_packets_.insert(make_pair(packets[i].first, unacked));
      } else {
        DeleteEnclosedFrame(&frames[i]);
      }
    }

    if (last_packet != NULL) {
      *last_packet = packets[packets.size() - 1].first;
    }
    if (data.size() == 0) {
      // We're done writing the data.   Exit the loop.
      // We don't make this a precondition beacuse we could have 0 bytes of data
      // if we're simply writing a fin.
      break;
    }
  }
  return QuicConsumedData(total_bytes_consumed, fin_consumed);
}

void QuicConnection::SendRstStream(QuicStreamId id,
                                   QuicErrorCode error,
                                   QuicStreamOffset offset) {
  queued_control_frames_.push_back(QuicFrame(
      new QuicRstStreamFrame(id, offset, error)));

  // Try to write immediately if possible.
  if (CanWrite(false)) {
    WriteData();
  }
}

void QuicConnection::ProcessUdpPacket(const IPEndPoint& self_address,
                                      const IPEndPoint& peer_address,
                                      const QuicEncryptedPacket& packet) {
  last_packet_revived_ = false;
  last_size_ = packet.length();
  framer_.ProcessPacket(self_address, peer_address, packet);

  MaybeProcessRevivedPacket();
}

bool QuicConnection::OnCanWrite() {
  write_blocked_ = false;

  WriteData();

  // If we've sent everything we had queued and we're still not blocked, let the
  // visitor know it can write more.
  if (!write_blocked_) {
    bool all_bytes_written = visitor_->OnCanWrite();
    // If the latest write caused a socket-level blockage, return false: we will
    // be rescheduled by the kernel.
    if (write_blocked_) {
      return false;
    }
    if (!all_bytes_written && !helper_->IsSendAlarmSet()) {
      // We're not write blocked, but some stream didn't write out all of its
      // bytes.  Register for 'immediate' resumption so we'll keep writing after
      // other quic connections have had a chance to use the socket.
      helper_->SetSendAlarm(QuicTime::Delta());
    }
  }

  return !write_blocked_;
}

bool QuicConnection::WriteData() {
  DCHECK_EQ(false, write_blocked_);
  // Serialize the ack and congestion frames before draining the pending queue.
  if (should_send_ack_) {
    queued_control_frames_.push_back(QuicFrame(&outgoing_ack_));
  }
  if (should_send_congestion_feedback_) {
    queued_control_frames_.push_back(QuicFrame(&outgoing_congestion_feedback_));
  }
  while (!queued_control_frames_.empty()) {
    size_t num_serialized;
    PacketPair pair = packet_creator_.SerializeFrames(
        queued_control_frames_, &num_serialized);
    // If any serialized frames need to be resent, add them to unacked_packets.
    QuicFrames unacked_frames;
    for (QuicFrames::const_iterator iter = queued_control_frames_.begin();
         iter != queued_control_frames_.begin() + num_serialized; ++iter) {
      if (ShouldResend(*iter)) {
        unacked_frames.push_back(*iter);
      }
    }
    if (!unacked_frames.empty()) {
      unacked_packets_.insert(make_pair(pair.first,
                                        new UnackedPacket(unacked_frames)));
    }
    queued_packets_.push_back(QueuedPacket(
        pair.first, pair.second, !unacked_frames.empty(), false));
    queued_control_frames_.erase(
        queued_control_frames_.begin(),
        queued_control_frames_.begin() + num_serialized);
  }
  should_send_ack_ = false;
  should_send_congestion_feedback_ = false;

  size_t num_queued_packets = queued_packets_.size() + 1;
  while (!write_blocked_ && !helper_->IsSendAlarmSet() &&
         !queued_packets_.empty()) {
    // Ensure that from one iteration of this loop to the next we
    // succeeded in sending a packet so we don't infinitely loop.
    // TODO(rch): clean up and close the connection if we really hit this.
    DCHECK_LT(queued_packets_.size(), num_queued_packets);
    num_queued_packets = queued_packets_.size();
    QueuedPacket p = queued_packets_.front();
    queued_packets_.pop_front();
    SendPacket(p.sequence_number, p.packet, p.resend, false, p.retransmit);
  }

  return !write_blocked_;
}

void QuicConnection::RecordPacketReceived(const QuicPacketHeader& header) {
  QuicPacketSequenceNumber sequence_number = header.packet_sequence_number;
  DCHECK(outgoing_ack_.received_info.IsAwaitingPacket(sequence_number));
  outgoing_ack_.received_info.RecordReceived(sequence_number);
}

bool QuicConnection::MaybeResendPacketForRTO(
    QuicPacketSequenceNumber sequence_number) {
  // If the packet hasn't been acked and we're getting truncated acks, ignore
  // any RTO for packets larger than the peer's largest observed packet; it may
  // have been received by the peer and just wasn't acked due to the ack frame
  // running out of space.
  if (received_truncated_ack_ &&
      sequence_number > peer_largest_observed_packet_ &&
      ContainsKey(unacked_packets_, sequence_number)) {
    return false;
  } else {
    MaybeResendPacket(sequence_number);
    return true;
  }
}

void QuicConnection::MaybeResendPacket(
    QuicPacketSequenceNumber sequence_number) {
  UnackedPacketMap::iterator it = unacked_packets_.find(sequence_number);

  if (it != unacked_packets_.end()) {
    UnackedPacket* unacked = it->second;
    // TODO(ianswett): Never change the sequence number of the connect packet.
    unacked_packets_.erase(it);
    // Re-packetize the frames with a new sequence number for resend.
    // Resent data packets do not use FEC, even when it's enabled.
    PacketPair packetpair = packet_creator_.SerializeAllFrames(unacked->frames);
    DVLOG(1) << "Resending unacked packet " << sequence_number << " as "
             << packetpair.first;
    unacked_packets_.insert(make_pair(packetpair.first, unacked));

    // Make sure if this was our least unacked packet, that we update our
    // outgoing ack.  If this wasn't the least unacked, this is a no-op.
    UpdateLeastUnacked(sequence_number);
    SendPacket(packetpair.first, packetpair.second, true, false, true);
  } else {
    DVLOG(2) << "alarm fired for " << sequence_number
             << " but it has been acked";
  }
}

bool QuicConnection::CanWrite(bool is_retransmit) {
  // TODO(ianswett): If the packet is a retransmit, the current send alarm may
  // be too long.
  if (write_blocked_ || helper_->IsSendAlarmSet()) {
    return false;
  }
  QuicTime::Delta delay = scheduler_->TimeUntilSend(is_retransmit);
  // If the scheduler requires a delay, then we can not send this packet now.
  if (!delay.IsZero() && !delay.IsInfinite()) {
    // TODO(pwestin): we need to handle delay.IsInfinite() seperately.
    helper_->SetSendAlarm(delay);
    return false;
  }
  return true;
}

bool QuicConnection::SendPacket(QuicPacketSequenceNumber sequence_number,
                                QuicPacket* packet,
                                bool should_resend,
                                bool force,
                                bool is_retransmit) {
  // If this packet is being forced, don't bother checking to see if we should
  // write, just write.
  if (!force) {
    // If we can't write, then simply queue the packet.
    if (!CanWrite(is_retransmit)) {
      queued_packets_.push_back(
          QueuedPacket(sequence_number, packet, should_resend, is_retransmit));
      return false;
    }
  }
  if (should_resend) {
    helper_->SetResendAlarm(sequence_number, DefaultResendTime());
    // The second case should never happen in the real world, but does here
    // because we sometimes send out of order to validate corner cases.
    if (outgoing_ack_.sent_info.least_unacked == 0 ||
        sequence_number < outgoing_ack_.sent_info.least_unacked) {
      outgoing_ack_.sent_info.least_unacked = sequence_number;
    }
  }

  scoped_ptr<QuicEncryptedPacket> encrypted(framer_.EncryptPacket(*packet));
  int error;
  DLOG(INFO) << "Sending packet : "
             << (should_resend ? "data bearing " : " ack only ")
             << "packet " << sequence_number;
  DCHECK(encrypted->length() <= kMaxPacketSize)
      << "Packet " << sequence_number << " will not be read; too large: "
      << packet->length() << " " << encrypted->length() << " " << outgoing_ack_;

  int rv = helper_->WritePacketToWire(*encrypted, &error);
  if (rv == -1) {
    if (error == ERR_IO_PENDING) {
      write_blocked_ = true;

      // TODO(rch): uncomment when we get non-blocking (and non-retrying)
      // UDP sockets.
      /*
      queued_packets_.push_front(
          QueuedPacket(sequence_number, packet, should_resend, is_retransmit));
      */
      return false;
    }
    // TODO(wtc): is it correct to fall through to return true?
  }

  time_of_last_packet_ = clock_->Now();
  DVLOG(1) << "last packet: " << time_of_last_packet_.ToMicroseconds();

  scheduler_->SentPacket(sequence_number, packet->length(), is_retransmit);
  delete packet;
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

void QuicConnection::SendAck() {
  helper_->ClearAckAlarm();

  if (!ContainsKey(unacked_packets_, outgoing_ack_.sent_info.least_unacked)) {
    // At some point, all packets were acked, and we set least_unacked to a
    // packet we will not resend.  Make sure we update it.
    UpdateLeastUnacked(outgoing_ack_.sent_info.least_unacked);
  }

  DVLOG(1) << "Sending ack " << outgoing_ack_;

  should_send_ack_ = true;

  if (collector_->GenerateCongestionFeedback(&outgoing_congestion_feedback_)) {
    DVLOG(1) << "Sending feedback " << outgoing_congestion_feedback_;
    should_send_congestion_feedback_ = true;
  }
  // Try to write immediately if possible.
  if (CanWrite(false)) {
    WriteData();
  }
}

void QuicConnection::MaybeProcessRevivedPacket() {
  QuicFecGroup* group = GetFecGroup();
  if (group == NULL || !group->CanRevive()) {
    return;
  }
  DCHECK(!revived_payload_.get());
  revived_payload_.reset(new char[kMaxPacketSize]);
  size_t len = group->Revive(&revived_header_, revived_payload_.get(),
                             kMaxPacketSize);
  group_map_.erase(last_header_.fec_group);
  delete group;

  last_packet_revived_ = true;
  framer_.ProcessRevivedPacket(revived_header_,
                               StringPiece(revived_payload_.get(), len));
  revived_payload_.reset();
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
  DLOG(INFO) << "Force closing with error " << QuicUtils::ErrorToString(error)
             << " (" << error << ")";
  QuicConnectionCloseFrame frame;
  frame.error_code = error;
  frame.ack_frame = outgoing_ack_;

  PacketPair packetpair = packet_creator_.CloseConnection(&frame);
  // There's no point in resending this: we're closing the connection.
  SendPacket(packetpair.first, packetpair.second, false, true, false);
  connected_ = false;
  visitor_->ConnectionClose(error, false);
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
  return !queued_packets_.empty() || should_send_ack_ ||
      should_send_congestion_feedback_;
}

bool QuicConnection::CheckForTimeout() {
  QuicTime now = clock_->Now();
  QuicTime::Delta delta = now.Subtract(time_of_last_packet_);
  DVLOG(1) << "last_packet " << time_of_last_packet_.ToMicroseconds()
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
