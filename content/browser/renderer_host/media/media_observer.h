// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_OBSERVER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_OBSERVER_H_
#pragma once

// A class may implement MediaObserver and register itself with ResourceContext
// to receive callbacks as media events occur.
class MediaObserver {
 public:
  // Called when an audio stream is set to playing or paused.
  virtual void OnSetAudioStreamPlaying(void* host, int32 render_view,
                                       int stream_id, bool playing) = 0;
  // Called when the volume of an audio stream is set.
  virtual void OnSetAudioStreamVolume(void* host, int32 render_view,
                                      int stream_id, double volume) = 0;
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_OBSERVER_H_
