// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_GLUE_UTILS_H_
#define JINGLE_GLUE_UTILS_H_

#include <string>

#include "net/base/ip_address_number.h"
#include "third_party/webrtc/base/ipaddress.h"

namespace net {
class IPEndPoint;
}  // namespace net

namespace rtc {
class SocketAddress;
}  // namespace rtc

namespace cricket {
class Candidate;
}  // namespace cricket

namespace jingle_glue {

// Chromium and libjingle represent socket addresses differently. The
// following two functions are used to convert addresses from one
// representation to another.
bool IPEndPointToSocketAddress(const net::IPEndPoint& ip_endpoint,
                               rtc::SocketAddress* address);
bool SocketAddressToIPEndPoint(const rtc::SocketAddress& address,
                               net::IPEndPoint* ip_endpoint);

rtc::IPAddress IPAddressNumberToIPAddress(
    const net::IPAddressNumber& ip_address_number);

// Helper functions to serialize and deserialize P2P candidates.
std::string SerializeP2PCandidate(const cricket::Candidate& candidate);
bool DeserializeP2PCandidate(const std::string& address,
                             cricket::Candidate* candidate);

}  // namespace jingle_glue

#endif  // JINGLE_GLUE_UTILS_H_
