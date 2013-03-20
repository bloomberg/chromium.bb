// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MOCK_MEDIA_OBSERVER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MOCK_MEDIA_OBSERVER_H_

#include <string>

#include "base/basictypes.h"
#include "content/browser/media/media_internals.h"
#include "content/public/browser/media_observer.h"
#include "media/base/media_log_event.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class MockMediaObserver : public MediaObserver {
 public:
  MockMediaObserver();
  virtual ~MockMediaObserver();

  MOCK_METHOD4(OnCaptureDevicesOpened,
               void(int render_process_id, int render_view_id,
                    const MediaStreamDevices& devices,
                    const base::Closure& close_callback));
  MOCK_METHOD3(OnCaptureDevicesClosed,
               void(int render_process_id, int render_view_id,
                    const MediaStreamDevices& devices));
  MOCK_METHOD1(OnAudioCaptureDevicesChanged,
               void(const MediaStreamDevices& devices));
  MOCK_METHOD1(OnVideoCaptureDevicesChanged,
                 void(const MediaStreamDevices& devices));
  MOCK_METHOD4(OnMediaRequestStateChanged,
               void(int render_process_id, int render_view_id,
                    const MediaStreamDevice& device,
                    const MediaRequestState state));
  MOCK_METHOD4(OnAudioStreamPlayingChanged,
               void(int render_process_id,
                    int render_view_id,
                    int stream_id,
                    bool playing));
};

class MockMediaInternals : public MediaInternals {
 public:
  MockMediaInternals();
  virtual ~MockMediaInternals();

  MOCK_METHOD2(OnDeleteAudioStream,
               void(void* host, int stream_id));
  MOCK_METHOD3(OnSetAudioStreamPlaying,
               void(void* host, int stream_id, bool playing));
  MOCK_METHOD3(OnSetAudioStreamStatus,
               void(void* host, int stream_id, const std::string& status));
  MOCK_METHOD3(OnSetAudioStreamVolume,
               void(void* host, int stream_id, double volume));
  MOCK_METHOD2(OnMediaEvent,
               void(int source, const media::MediaLogEvent& event));
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MOCK_MEDIA_OBSERVER_H_
