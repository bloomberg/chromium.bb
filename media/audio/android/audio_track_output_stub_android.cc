// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/audio_track_output_android.h"

// This file was added because there is no Java environment in
// upstream yet, audio_track_output_android.cc should be used in
// downstream.
// TODO(michaelbai): Remove this file once Java environment ready.

// static
AudioOutputStream* AudioTrackOutputStream::MakeStream(
    const AudioParameters& params) {
  return NULL;
}
