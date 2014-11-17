// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVTOOLS_BRIDGE_RTC_CONFIGURATION_H_
#define COMPONENTS_DEVTOOLS_BRIDGE_RTC_CONFIGURATION_H_

#include <string>

#include "base/memory/scoped_ptr.h"

namespace devtools_bridge {

/**
 * WebRTC configuration for DevTools Bridge.
 */
class RTCConfiguration {
 public:
  class Impl;

  RTCConfiguration() {}
  virtual ~RTCConfiguration() {}

  virtual void AddIceServer(
      const std::string& uri,
      const std::string& username,
      const std::string& credential) = 0;

  virtual const Impl& impl() const = 0;

  static scoped_ptr<RTCConfiguration> CreateInstance();

 private:
  DISALLOW_COPY_AND_ASSIGN(RTCConfiguration);
};

}  // namespace devtools_bridge

#endif  // COMPONENTS_DEVTOOLS_BRIDGE_RTC_CONFIGURATION_H_
