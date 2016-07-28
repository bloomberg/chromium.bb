// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ANDROID_SYNC_JNI_REGISTRAR_H_
#define COMPONENTS_SYNC_ANDROID_SYNC_JNI_REGISTRAR_H_

#include <jni.h>

#include "components/sync/base/sync_export.h"

namespace syncer {

SYNC_EXPORT bool RegisterSyncJni(JNIEnv* env);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ANDROID_SYNC_JNI_REGISTRAR_H_
