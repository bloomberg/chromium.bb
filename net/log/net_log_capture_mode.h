// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_LOG_NET_LOG_CAPTURE_MODE_H_
#define NET_LOG_NET_LOG_CAPTURE_MODE_H_

#include <stdint.h>
#include <string>

#include "net/base/net_export.h"

namespace net {

// NetLogCaptureMode specifies the granularity of events that should be emitted
// to the log. It is a simple wrapper around an integer, so it should be passed
// to functions by value rather than by reference.
class NET_EXPORT NetLogCaptureMode {
 public:
  // NOTE: Default assignment and copy constructor are OK.

  // The default constructor creates an empty capture mode (equivalent to
  // None()).
  NetLogCaptureMode();

  // Constructs a capture mode which logs NOTHING.
  //    enabled() --> false
  //    include_cookies_and_credentials() --> false
  //    include_socket_bytes() --> false
  static NetLogCaptureMode None();

  // Constructs a capture mode which logs basic events and event parameters.
  //    enabled() --> true
  //    include_cookies_and_credentials() --> false
  //    include_socket_bytes() --> false
  static NetLogCaptureMode Default();

  // Constructs a capture mode which logs basic events, and additionally makes
  // no effort to strip cookies and credentials.
  //    enabled() --> true
  //    include_cookies_and_credentials() --> true
  //    include_socket_bytes() --> false
  static NetLogCaptureMode IncludeCookiesAndCredentials();

  // Constructs a capture mode which logs the data sent/received from sockets.
  //    enabled() --> true
  //    include_cookies_and_credentials() --> true
  //    include_socket_bytes() --> true
  static NetLogCaptureMode IncludeSocketBytes();

  // Returns a capture mode that contains the maximal set of capabilities
  // between |mode1| and |mode2|.
  static NetLogCaptureMode Max(NetLogCaptureMode mode1,
                               NetLogCaptureMode mode2);

  // If enabled() is true, then _something_ is being captured.
  bool enabled() const;

  // If include_cookies_and_credentials() is true , then it is OK to log
  // events which contain cookies, credentials or other privacy sensitive data.
  bool include_cookies_and_credentials() const;

  // If include_socket_bytes() is true, then it is OK to output the actual
  // bytes read/written from the network, even if it contains private data.
  bool include_socket_bytes() const;

  bool operator==(NetLogCaptureMode mode) const;
  bool operator!=(NetLogCaptureMode mode) const;

  int32_t ToInternalValueForTesting() const;

 private:
  // NetLog relies on the internal value of NetLogCaptureMode being an integer,
  // so it can be read/written atomically across thread.
  friend class NetLog;

  explicit NetLogCaptureMode(uint32_t value);

  static NetLogCaptureMode FromInternalValue(int32_t value);
  int32_t ToInternalValue() const;

  int32_t value_;
};

}  // namespace net

#endif  // NET_LOG_NET_LOG_CAPTURE_MODE_H_
