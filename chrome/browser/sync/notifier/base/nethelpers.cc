// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/base/nethelpers.h"

namespace notifier {

hostent* SafeGetHostByName(const char* hostname, hostent* host,
                           char* buffer, size_t buffer_len,
                           int* herrno) {
  hostent* result = NULL;
#if WIN32
  result = gethostbyname(hostname);
  if (!result) {
    *herrno = WSAGetLastError();
  }
#elif OS_LINUX
  gethostbyname_r(hostname, host, buffer, buffer_len, &result, herrno);
#elif OSX
  result = getipnodebyname(hostname, AF_INET, AI_DEFAULT, herrno);
#else
#error "I don't know how to do gethostbyname safely on your system."
#endif
  return result;
}

// This function should mirror the above function, and free any resources
// allocated by the above.
void FreeHostEnt(hostent* host) {
#if WIN32
  // No need to free anything, struct returned is static memory.
#elif OS_LINUX
  // No need to free anything, we pass in a pointer to a struct.
#elif OSX
  freehostent(host);
#else
#error "I don't know how to free a hostent on your system."
#endif
}

}  // namespace notifier
