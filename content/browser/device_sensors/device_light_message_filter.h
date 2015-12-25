// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_SENSORS_DEVICE_LIGHT_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_DEVICE_SENSORS_DEVICE_LIGHT_MESSAGE_FILTER_H_

#include "base/macros.h"
#include "content/public/browser/browser_message_filter.h"

namespace content {

class DeviceLightMessageFilter : public BrowserMessageFilter {
 public:
  DeviceLightMessageFilter();

  // BrowserMessageFilter implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  ~DeviceLightMessageFilter() override;

  void OnDeviceLightStartPolling();
  void OnDeviceLightStopPolling();
  void DidStartDeviceLightPolling();

  bool is_started_;

  DISALLOW_COPY_AND_ASSIGN(DeviceLightMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_SENSORS_DEVICE_LIGHT_MESSAGE_FILTER_H_
