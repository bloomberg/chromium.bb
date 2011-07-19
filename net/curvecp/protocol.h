// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CURVECP_PROTOCOL_H_
#define NET_CURVECP_PROTOCOL_H_
#pragma once

// Mike's thoughts on the protocol:
//   1) the packet layer probably should have a "close" mechanism.  although
//      some clients won't use it, it is nice to have.
//
//   2) when do we re "initiate"?  maybe connections should have an inferred
//      lifetime, and client should always re-initiate after 1min?
//
//   3) An initiate packet can cary 640B of data.  But how does the app layer
//      know that only 640B of data is available there?  This is a bit awkward?

#include "base/basictypes.h"

namespace net {

typedef unsigned char uchar;

// Messages can range in size from 16 to 1088 bytes.
const int kMinMessageLength = 16;
const int kMaxMessageLength = 1088;

// Maximum packet size.
const int kMaxPacketLength = kMaxMessageLength + 96;



// Packet layer.

// All Packets start with an 8 byte identifier.
struct Packet {
  char id[8];
};

// A HelloPacket is sent from the client to the server to establish a secure
// cookie to use when communicating with a server.
struct HelloPacket : Packet {
  uchar server_extension[16];
  uchar client_extension[16];
  uchar client_shortterm_public_key[32];
  uchar zero[64];
  uchar nonce[8];
  uchar box[80];
};

// A CookiePacket is sent from the server to the client in response to a
// HelloPacket.
struct CookiePacket : Packet {
  uchar client_extension[16];
  uchar server_extension[16];
  uchar nonce[16];
  uchar box[144];
};

// An InitiatePacket is sent from the client to the server to initiate
// the connection once a cookie has been exchanged.
struct InitiatePacket : Packet {
  uchar server_extension[16];
  uchar client_extension[16];
  uchar client_shortterm_public_key[32];
  uchar server_cookie[96];
  uchar initiate_nonce[8];

  uchar client_longterm_public_key[32];
  uchar nonce[16];
  uchar box[48];
  uchar server_name[256];

  // A message is carried here.
};

// A ClientMessagePacket contains a Message from the client to the server.
struct ClientMessagePacket : Packet {
  uchar server_extension[16];
  uchar client_extension[16];
  uchar client_shortterm_public_key[32];
  uchar nonce[8];

  // A message is carried here of at least 16 bytes.
};

// A ServerMessagePacket contains a Message from the server to the client.
struct ServerMessagePacket : Packet {
  uchar client_extension[16];
  uchar server_extension[16];
  uchar nonce[8];

  // A message is carried here of at least 16 bytes.
};




// Messaging layer.

struct Message {
  // If message_id is all zero, this Message is a pure ACK message.
  uchar message_id[4];
  // For regular messages going back and forth, last_message_received will be
  // zero.  For an ACK, it will be filled in and can be used to compute RTT.
  uchar last_message_received[4];
  uchar acknowledge_1[8];  // bytes acknowledged in the first range.
  uchar gap_1[4];          // gap between the first range and the second range.
  uchar acknowledge_2[2];  // bytes acknowledged in the second range.
  uchar gap_2[2];          // gap between the second range and the third range.
  uchar acknowledge_3[2];  // bytes acknowledged in the third range.
  uchar gap_3[2];          // gap between the third range and the fourth range.
  uchar acknowledge_4[2];  // bytes acknowledged in the fourth range.
  uchar gap_4[2];          // gap between the fourth range and the fifth range.
  uchar acknowledge_5[2];  // bytes acknowledged in the fifth range.
  uchar gap_5[2];          // gap between the fifth range and the sixth range.
  uchar acknowledge_6[2];  // bytes acknowledged in the sixth range.
  union {
    struct {
      unsigned short unused:4;
      unsigned short fail:1;
      unsigned short success:1;
      unsigned short length:10;
    } bits;
    uchar val[2];
  } size;
  uchar position[8];
  uchar padding[16];

  bool is_ack() { return *(reinterpret_cast<int32*>(message_id)) == 0; }
};

// Connection state
// TODO(mbelshe) move this to the messenger.
struct ConnectionState {
  uchar client_shortterm_public_key[32];

  // Fields we'll need going forward:
  //
  // uchar secret_key_client_short_server_short[32];
  // crypto_uint64 received_nonce;
  // crypto_uint64 sent_nonce;
  // uchar client_extension[16];
  // uchar server_extension[16];
};

void uint16_pack(uchar* dest, uint16 val);
uint16 uint16_unpack(uchar* src);
void uint32_pack(uchar* dest, uint32 val);
uint32 uint32_unpack(uchar* src);
void uint64_pack(uchar* dest, uint64 val);
uint64 uint64_unpack(uchar* src);


}  // namespace net

#endif  // NET_CURVECP_PROTOCOL_H_
