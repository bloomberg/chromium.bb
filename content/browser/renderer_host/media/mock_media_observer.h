// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MOCK_MEDIA_OBSERVER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MOCK_MEDIA_OBSERVER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "content/browser/renderer_host/media/media_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockMediaObserver : public MediaObserver {
 public:
  MockMediaObserver();
  virtual ~MockMediaObserver();

  MOCK_METHOD3(OnDeleteAudioStream,
               void(void* host, int32 render_view, int stream_id));
  MOCK_METHOD4(OnSetAudioStreamPlaying,
               void(void* host, int32 render_view, int stream_id,
                    bool playing));
  MOCK_METHOD4(OnSetAudioStreamStatus,
               void(void* host, int32 render_view, int stream_id,
                    const std::string& status));
  MOCK_METHOD4(OnSetAudioStreamVolume,
               void(void* host, int32 render_view, int stream_id,
                    double volume));
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MOCK_MEDIA_OBSERVER_H_
