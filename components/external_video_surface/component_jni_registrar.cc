// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/external_video_surface/component_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "components/external_video_surface/browser/android/external_video_surface_container_impl.h"

namespace external_video_surface {

static base::android::RegistrationMethod kComponentRegisteredMethods[] = {
  { "ExternalVideoSurface", ExternalVideoSurfaceContainerImpl::RegisterJni },
};

bool RegisterExternalVideoSurfaceJni(JNIEnv* env) {
  return RegisterNativeMethods(env,
      kComponentRegisteredMethods, arraysize(kComponentRegisteredMethods));
}

} // namespace external_video_surface

