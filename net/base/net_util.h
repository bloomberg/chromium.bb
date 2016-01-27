// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NET_UTIL_H_
#define NET_BASE_NET_UTIL_H_

#include <stdint.h>

#include <string>

#include "base/strings/string_piece.h"
#include "net/base/net_export.h"

namespace net {

class AddressList;

// Returns the hostname of the current system. Returns empty string on failure.
NET_EXPORT std::string GetHostName();

// Resolves a local hostname (such as "localhost" or "localhost6") into
// IP endpoints with the given port. Returns true if |host| is a local
// hostname and false otherwise. Special IPv6 names (e.g. "localhost6")
// will resolve to an IPv6 address only, whereas other names will
// resolve to both IPv4 and IPv6.
NET_EXPORT_PRIVATE bool ResolveLocalHostname(base::StringPiece host,
                                             uint16_t port,
                                             AddressList* address_list);

// Returns true if |host| is one of the local hostnames
// (e.g. "localhost") or IP addresses (IPv4 127.0.0.0/8 or IPv6 ::1).
//
// Note that this function does not check for IP addresses other than
// the above, although other IP addresses may point to the local
// machine.
NET_EXPORT bool IsLocalhost(base::StringPiece host);

}  // namespace net

#endif  // NET_BASE_NET_UTIL_H_
