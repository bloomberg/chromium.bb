// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ANDROID_REMOTE_RECORD_CAST_ACTION_H_
#define CHROME_BROWSER_MEDIA_ANDROID_REMOTE_RECORD_CAST_ACTION_H_

#include <jni.h>

namespace remote_media {

// Registers the native methods through jni
bool RegisterRecordCastAction(JNIEnv* env);

} // namespace remote_media

#endif  // CHROME_BROWSER_MEDIA_ANDROID_REMOTE_RECORD_CAST_ACTION_H_
