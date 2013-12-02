// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is only included in ssl_client_socket_nss.cc and
// ssl_server_socket_nss.cc to share common functions of NSS.

#ifndef NET_SOCKET_NSS_SSL_UTIL_H_
#define NET_SOCKET_NSS_SSL_UTIL_H_

#include <prerror.h>

#include "base/basictypes.h"
#include "net/base/net_export.h"

namespace net {

class BoundNetLog;

// Initalize NSS SSL library.
NET_EXPORT void EnsureNSSSSLInit();

// Log a failed NSS funcion call.
void LogFailedNSSFunction(const BoundNetLog& net_log,
                          const char* function,
                          const char* param);

// Map network error code to NSS error code.
PRErrorCode MapErrorToNSS(int result);

// GetNSSCipherOrder either returns NULL, to indicate that no special cipher
// order is required, or it returns a pointer to an array of cipher suite ids.
// On return, |*out_length| is set to the length of the returned array.
//
// The array, if not NULL, indicates the preferred order of cipher suites but
// inclusion in the array does not mean that the given cipher suite should
// necessarily be enabled.
const uint16* GetNSSCipherOrder(size_t* out_length);

// Map NSS error code to network error code.
int MapNSSError(PRErrorCode err);

}  // namespace net

#endif  // NET_SOCKET_NSS_SSL_UTIL_H_
