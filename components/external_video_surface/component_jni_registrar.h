// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXTERNAL_VIDEO_SURFACE_COMPONENT_JNI_REGISTRAR_H_
#define COMPONENTS_EXTERNAL_VIDEO_SURFACE_COMPONENT_JNI_REGISTRAR_H_

#include <jni.h>

namespace external_video_surface {

// Register all JNI bindings necessary for the external_video_surface component.
bool RegisterExternalVideoSurfaceJni(JNIEnv* env);

}  // namespace external_video_surface

#endif  // COMPONENTS_EXTERNAL_VIDEO_SURFACE_COMPONENT_JNI_REGISTRAR_H_
