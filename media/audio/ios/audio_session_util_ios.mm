// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/ios/audio_session_util_ios.h"

#include <AVFoundation/AVFoundation.h>

#include "base/logging.h"

namespace media {

bool InitAudioSessionIOS() {
  static bool kSessionInitialized = false;
  static dispatch_once_t once = 0;
  dispatch_once(&once, ^{
      OSStatus error = AudioSessionInitialize(NULL, NULL, NULL, NULL);
      if (error != kAudioSessionNoError)
        DLOG(ERROR) << "AudioSessionInitialize OSStatus error: " << error;
      BOOL result = [[AVAudioSession sharedInstance]
          setCategory:AVAudioSessionCategoryPlayAndRecord
                error:nil];
      if (!result)
        DLOG(ERROR) << "AVAudioSession setCategory failed";
      UInt32 allowMixing = true;
      AudioSessionSetProperty(
          kAudioSessionProperty_OverrideCategoryMixWithOthers,
          sizeof(allowMixing), &allowMixing);
      UInt32 defaultToSpeaker = true;
      AudioSessionSetProperty(
          kAudioSessionProperty_OverrideCategoryDefaultToSpeaker,
          sizeof(defaultToSpeaker),
          &defaultToSpeaker);
      // Speech input cannot be used if either of these two conditions fail.
      kSessionInitialized = (error == kAudioSessionNoError) && result;
  });
  return kSessionInitialized;
}

}  // namespace media
