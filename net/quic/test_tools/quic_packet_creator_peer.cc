// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/quic_packet_creator_peer.h"

#include "net/quic/quic_packet_creator.h"

namespace net {
namespace test {

// static
bool QuicPacketCreatorPeer::SendVersionInPacket(QuicPacketCreator* creator) {
  return creator->send_version_in_packet_;
}

// static
void QuicPacketCreatorPeer::SetSendVersionInPacket(
    QuicPacketCreator* creator,
    bool send_version_in_packet) {
  creator->send_version_in_packet_ = send_version_in_packet;
}

// static
void QuicPacketCreatorPeer::SetPacketNumberLength(
    QuicPacketCreator* creator,
    QuicPacketNumberLength packet_number_length) {
  creator->packet_number_length_ = packet_number_length;
}

// static
void QuicPacketCreatorPeer::SetNextPacketNumberLength(
    QuicPacketCreator* creator,
    QuicPacketNumberLength next_packet_number_length) {
  creator->next_packet_number_length_ = next_packet_number_length;
}

// static
QuicPacketNumberLength QuicPacketCreatorPeer::NextPacketNumberLength(
    QuicPacketCreator* creator) {
  return creator->next_packet_number_length_;
}

// static
QuicPacketNumberLength QuicPacketCreatorPeer::GetPacketNumberLength(
    QuicPacketCreator* creator) {
  return creator->packet_number_length_;
}

void QuicPacketCreatorPeer::SetPacketNumber(QuicPacketCreator* creator,
                                            QuicPacketNumber s) {
  creator->packet_number_ = s;
}

// static
void QuicPacketCreatorPeer::FillPacketHeader(QuicPacketCreator* creator,
                                             QuicFecGroupNumber fec_group,
                                             bool fec_flag,
                                             QuicPacketHeader* header) {
  creator->FillPacketHeader(fec_group, fec_flag, header);
}

// static
size_t QuicPacketCreatorPeer::CreateStreamFrame(QuicPacketCreator* creator,
                                                QuicStreamId id,
                                                QuicIOVector iov,
                                                size_t iov_offset,
                                                QuicStreamOffset offset,
                                                bool fin,
                                                QuicFrame* frame) {
  return creator->CreateStreamFrame(id, iov, iov_offset, offset, fin, frame);
}

// static
bool QuicPacketCreatorPeer::IsFecProtected(QuicPacketCreator* creator) {
  return creator->fec_protect_;
}

// static
bool QuicPacketCreatorPeer::IsFecEnabled(QuicPacketCreator* creator) {
  return creator->max_packets_per_fec_group_ > 0;
}

// static
void QuicPacketCreatorPeer::StartFecProtectingPackets(
    QuicPacketCreator* creator) {
  creator->StartFecProtectingPackets();
}

// static
void QuicPacketCreatorPeer::StopFecProtectingPackets(
    QuicPacketCreator* creator) {
  creator->StopFecProtectingPackets();
}

// static
SerializedPacket QuicPacketCreatorPeer::SerializeFec(QuicPacketCreator* creator,
                                                     char* buffer,
                                                     size_t buffer_len) {
  return creator->SerializeFec(buffer, buffer_len);
}

// static
void QuicPacketCreatorPeer::ResetFecGroup(QuicPacketCreator* creator) {
  creator->ResetFecGroup();
}

// static
QuicTime::Delta QuicPacketCreatorPeer::GetFecTimeout(
    QuicPacketCreator* creator) {
  return creator->fec_timeout_;
}

// static
float QuicPacketCreatorPeer::GetRttMultiplierForFecTimeout(
    QuicPacketCreator* creator) {
  return creator->rtt_multiplier_for_fec_timeout_;
}

// static
EncryptionLevel QuicPacketCreatorPeer::GetEncryptionLevel(
    QuicPacketCreator* creator) {
  return creator->encryption_level_;
}

// static
QuicPathId QuicPacketCreatorPeer::GetCurrentPath(QuicPacketCreator* creator) {
  return creator->current_path_;
}

}  // namespace test
}  // namespace net
