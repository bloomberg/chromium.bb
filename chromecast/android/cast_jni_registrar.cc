// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/android/cast_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/macros.h"
#include "chromecast/base/android/system_time_change_notifier_android.h"
#include "chromecast/base/chromecast_config_android.h"
#include "chromecast/chromecast_features.h"

#if BUILDFLAG(IS_CAST_USING_CMA_BACKEND)
#include "chromecast/media/cma/backend/android/audio_sink_android_audiotrack_impl.h"
#include "chromecast/media/cma/backend/android/volume_control_android.h"
#endif

namespace chromecast {
namespace android {

namespace {

static base::android::RegistrationMethod kMethods[] = {
    {"ChromecastConfigAndroid", ChromecastConfigAndroid::RegisterJni},
    {"SystemTimeChangeNotifierAndroid",
     SystemTimeChangeNotifierAndroid::RegisterJni},
#if BUILDFLAG(IS_CAST_USING_CMA_BACKEND)
    {"AudioSinkAudioTrackImpl",
     media::AudioSinkAndroidAudioTrackImpl::RegisterJni},
    {"VolumeControlAndroid", media::VolumeControlAndroid::RegisterJni},
#endif
};

}  // namespace

bool RegisterJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kMethods, arraysize(kMethods));
}

}  // namespace android
}  // namespace chromecast
