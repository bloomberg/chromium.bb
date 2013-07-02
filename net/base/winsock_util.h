// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_WINSOCK_UTIL_H_
#define NET_BASE_WINSOCK_UTIL_H_

#include <winsock2.h>

#include "net/base/net_export.h"

namespace net {

// Assert that the (manual-reset) event object is not signaled.
void AssertEventNotSignaled(WSAEVENT hEvent);

// If the (manual-reset) event object is signaled, resets it and returns true.
// Otherwise, does nothing and returns false.  Called after a Winsock function
// succeeds synchronously
//
// Our testing shows that except in rare cases (when running inside QEMU),
// the event object is already signaled at this point, so we call this method
// to avoid a context switch in common cases.  This is just a performance
// optimization.  The code still works if this function simply returns false.
bool ResetEventIfSignaled(WSAEVENT hEvent);

// Interface to create Windows Socket.
// Usually such factories are used for testing purposes, which is not true in
// this case. This interface is used to substitute WSASocket to make possible
// execution of some network code in sandbox.
class NET_EXPORT PlatformSocketFactory {
 public:
  PlatformSocketFactory() {}
  virtual ~PlatformSocketFactory() {}

  // Creates Windows socket. See WSASocket documentation of parameters.
  virtual SOCKET CreateSocket(int family, int type, int protocol) = 0;

  // Replace WSASocket with given factory. The factory will be used by
  // CreatePlatformSocket.
  static void SetInstance(PlatformSocketFactory* factory);
};

// Creates Windows Socket. See WSASocket documentation of parameters.
SOCKET CreatePlatformSocket(int family, int type, int protocol);

}  // namespace net

#endif  // NET_BASE_WINSOCK_UTIL_H_
