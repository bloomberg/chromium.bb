// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ANDROID_MODEL_TYPE_HELPER_H_
#define COMPONENTS_SYNC_ANDROID_MODEL_TYPE_HELPER_H_

#include <jni.h>

namespace syncer {

bool RegisterModelTypeHelperJni(JNIEnv* env);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ANDROID_MODEL_TYPE_HELPER_H_
