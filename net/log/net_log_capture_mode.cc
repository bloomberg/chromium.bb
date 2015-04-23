// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/log/net_log_capture_mode.h"

#include <algorithm>

namespace net {

namespace {

// Integer representation for the capture mode. The numeric value is depended on
// for method of NetLogCaptureMode, which expect that higher values imply more
// capabilities.
enum InternalValue {
  // Don't log any events.
  NONE,

  // Log all events, but do not include the actual transferred bytes and
  // remove cookies and HTTP credentials.
  DEFAULT,

  // Log all events, but do not include the actual transferred bytes as
  // parameters for bytes sent/received events.
  INCLUDE_COOKIES_AND_CREDENTIALS,

  // Log everything possible, even if it is slow and memory expensive.
  // Includes logging of transferred bytes.
  INCLUDE_SOCKET_BYTES,
};

}  // namespace

// Default constructor creates an empty capture mode.
NetLogCaptureMode::NetLogCaptureMode() : NetLogCaptureMode(NONE) {
}

NetLogCaptureMode NetLogCaptureMode::None() {
  return NetLogCaptureMode(NONE);
}

NetLogCaptureMode NetLogCaptureMode::Default() {
  return NetLogCaptureMode(DEFAULT);
}

NetLogCaptureMode NetLogCaptureMode::IncludeCookiesAndCredentials() {
  return NetLogCaptureMode(INCLUDE_COOKIES_AND_CREDENTIALS);
}

NetLogCaptureMode NetLogCaptureMode::IncludeSocketBytes() {
  return NetLogCaptureMode(INCLUDE_SOCKET_BYTES);
}

NetLogCaptureMode NetLogCaptureMode::Max(NetLogCaptureMode mode1,
                                         NetLogCaptureMode mode2) {
  return NetLogCaptureMode(std::max(mode1.value_, mode2.value_));
}

bool NetLogCaptureMode::enabled() const {
  return value_ != NONE;
}

bool NetLogCaptureMode::include_cookies_and_credentials() const {
  return value_ >= INCLUDE_COOKIES_AND_CREDENTIALS;
}

bool NetLogCaptureMode::include_socket_bytes() const {
  return value_ >= INCLUDE_SOCKET_BYTES;
}

bool NetLogCaptureMode::operator==(NetLogCaptureMode mode) const {
  return value_ == mode.value_;
}

bool NetLogCaptureMode::operator!=(NetLogCaptureMode mode) const {
  return !(*this == mode);
}

int32_t NetLogCaptureMode::ToInternalValueForTesting() const {
  return ToInternalValue();
}

NetLogCaptureMode::NetLogCaptureMode(uint32_t value) : value_(value) {
}

NetLogCaptureMode NetLogCaptureMode::FromInternalValue(int32_t value) {
  return NetLogCaptureMode(value);
}

int32_t NetLogCaptureMode::ToInternalValue() const {
  return value_;
}

}  // namespace net
