// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_SENSORS_DEVICE_ORIENTATION_ABSOLUTE_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_DEVICE_SENSORS_DEVICE_ORIENTATION_ABSOLUTE_MESSAGE_FILTER_H_

#include "base/macros.h"
#include "content/public/browser/browser_message_filter.h"

namespace content {

class DeviceOrientationAbsoluteMessageFilter : public BrowserMessageFilter {
 public:
  DeviceOrientationAbsoluteMessageFilter();

  // BrowserMessageFilter implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  ~DeviceOrientationAbsoluteMessageFilter() override;

  void OnStartPolling();
  void OnStopPolling();
  void DidStartPolling();

  bool is_started_;

  DISALLOW_COPY_AND_ASSIGN(DeviceOrientationAbsoluteMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_SENSORS_DEVICE_ORIENTATION_ABSOLUTE_MESSAGE_FILTER_H_
