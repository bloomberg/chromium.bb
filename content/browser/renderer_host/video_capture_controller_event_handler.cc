// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/video_capture_controller_event_handler.h"

VideoCaptureControllerID::VideoCaptureControllerID()
    :routing_id(0),
     device_id(0) {}

VideoCaptureControllerID::VideoCaptureControllerID(int32 rid, int did)
    :routing_id(rid),
     device_id(did) {}

VideoCaptureControllerID::~VideoCaptureControllerID() {}

bool VideoCaptureControllerID::operator<(
    const VideoCaptureControllerID& vc) const {
  return this->routing_id < vc.routing_id && this->device_id < vc.device_id;
}
