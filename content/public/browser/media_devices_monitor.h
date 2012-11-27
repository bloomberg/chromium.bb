// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MEDIA_DEVICES_MONITOR_H_
#define CONTENT_PUBLIC_BROWSER_MEDIA_DEVICES_MONITOR_H_

#include "content/common/content_export.h"

namespace content {

// Called to ensure the MediaStreamManager has started monitoring the capture
// devices, this will trigger OnAudioCaptureDevicesChanged() and
// OnVideoCaptureDevicesChanged() callbacks.
// This must be called on the IO thread.
CONTENT_EXPORT void EnsureMonitorCaptureDevices();

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MEDIA_DEVICES_MONITOR_H_
