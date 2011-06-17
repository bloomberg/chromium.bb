// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEDIA_AUDIO_STREAM_STATE_H_
#define CONTENT_COMMON_MEDIA_AUDIO_STREAM_STATE_H_
#pragma once

// Current status of the audio output stream in the browser process. Browser
// sends information about the current playback state and error to the
// renderer process using this type.
enum AudioStreamState {
  kAudioStreamPlaying,
  kAudioStreamPaused,
  kAudioStreamError
};

#endif  // CONTENT_COMMON_MEDIA_AUDIO_STREAM_STATE_H_
