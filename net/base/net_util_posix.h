// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NET_UTIL_POSIX_H_
#define NET_BASE_NET_UTIL_POSIX_H_

// This file is only used to expose some of the internals of
// net_util_posix.cc to net_util_linux.cc and net_util_mac.cc.

#include <string>

struct sockaddr;

namespace net {
namespace internal {
#if !defined(OS_NACL)
bool ShouldIgnoreInterface(const std::string& name, int policy);
bool IsLoopbackOrUnspecifiedAddress(const sockaddr* addr);
#endif  // !OS_NACL

}  // namespace internal
}  // namespace net

#endif  // NET_BASE_NET_UTIL_POSIX_H_
