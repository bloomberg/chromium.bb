// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_BASE_NETHELPERS_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_BASE_NETHELPERS_H_

#include "base/basictypes.h"

#if defined(OS_POSIX)
#include <cstddef>
#include <netdb.h>
#elif defined(OS_WIN)
#include <winsock2.h>
#endif

namespace notifier {

hostent* SafeGetHostByName(const char* hostname, hostent* host,
                           char* buffer, size_t buffer_len,
                           int* herrno);

void FreeHostEnt(hostent* host);

}  // namespace notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_BASE_NETHELPERS_H_
