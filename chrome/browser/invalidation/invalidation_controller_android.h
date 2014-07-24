// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INVALIDATION_INVALIDATION_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_INVALIDATION_INVALIDATION_CONTROLLER_ANDROID_H_

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "components/invalidation/invalidation_util.h"

namespace invalidation {

// Controls invalidation registration on Android. This class is a wrapper for
// the Java class org.chromium.sync.notifier.InvalidationController.
class InvalidationControllerAndroid {
 public:
  InvalidationControllerAndroid();
  virtual ~InvalidationControllerAndroid();

  // Sets object ids for which the invalidation client should register for
  // notification.
  virtual void SetRegisteredObjectIds(const syncer::ObjectIdSet& ids);

  // Asks the Java code to return the client ID it chose to use.
  std::string GetInvalidatorClientId();

 private:
  // The Java invalidation controller.
  base::android::ScopedJavaGlobalRef<jobject> invalidation_controller_;

  DISALLOW_COPY_AND_ASSIGN(InvalidationControllerAndroid);
};

bool RegisterInvalidationController(JNIEnv* env);

}  // namespace invalidation

#endif  // CHROME_BROWSER_INVALIDATION_INVALIDATION_CONTROLLER_ANDROID_H_
