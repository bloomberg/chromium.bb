// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_ORIENTATION_MOTION_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_DEVICE_ORIENTATION_MOTION_MESSAGE_FILTER_H_

#include <map>

#include "content/browser/device_orientation/message_filter.h"

namespace content {

class MotionMessageFilter : public DeviceOrientationMessageFilter {
 public:
  MotionMessageFilter();

  // DeviceOrientationMessageFilter implementation.
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

 private:
  virtual ~MotionMessageFilter();

  DISALLOW_COPY_AND_ASSIGN(MotionMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_ORIENTATION_MOTION_MESSAGE_FILTER_H_
