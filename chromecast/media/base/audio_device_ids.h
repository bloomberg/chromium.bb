// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_BASE_AUDIO_DEVICE_IDS_H_
#define CHROMECAST_MEDIA_BASE_AUDIO_DEVICE_IDS_H_

namespace chromecast {
namespace media {

extern const char kAlarmAudioDeviceId[];
extern const char kEarconAudioDeviceId[];
extern const char kTtsAudioDeviceId[];

const int kDefaultAudioStreamType = 0;
const int kAlarmAudioStreamType = 1;
const int kTtsAudioStreamType = 2;

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_BASE_AUDIO_DEVICE_IDS_H_
