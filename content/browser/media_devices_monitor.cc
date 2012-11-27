// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/media_devices_monitor.h"

#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/public/browser/browser_thread.h"

namespace content {

void EnsureMonitorCaptureDevices() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Post a EnumerateDevices() API to MSM to start the monitoring.
  BrowserMainLoop::GetMediaStreamManager()->EnumerateDevices(
      NULL, -1, -1, MEDIA_DEVICE_AUDIO_CAPTURE, GURL());
}

}  // namespace content
