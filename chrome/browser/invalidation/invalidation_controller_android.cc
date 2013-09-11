// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/invalidation/invalidation_controller_android.h"

#include <string>
#include <vector>

#include "base/android/jni_array.h"
#include "google/cacheinvalidation/include/types.h"
#include "sync/jni/InvalidationController_jni.h"

namespace invalidation {

InvalidationControllerAndroid::InvalidationControllerAndroid() {}

InvalidationControllerAndroid::~InvalidationControllerAndroid() {}

void InvalidationControllerAndroid::SetRegisteredObjectIds(
    const syncer::ObjectIdSet& ids) {
  // Get a reference to the Java invalidation controller.
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);
  if (invalidation_controller_.is_null()) {
    invalidation_controller_.Reset(Java_InvalidationController_get(
        env,
        base::android::GetApplicationContext()));
  }

  // To call the corresponding method on the Java invalidation controller, split
  // the object ids into object source and object name arrays.
  std::vector<int> sources;
  std::vector<std::string> names;
  syncer::ObjectIdSet::const_iterator id;
  for (id = ids.begin(); id != ids.end(); ++id) {
    sources.push_back(id->source());
    names.push_back(id->name());
  }

  Java_InvalidationController_setRegisteredObjectIds(
      env,
      invalidation_controller_.obj(),
      base::android::ToJavaIntArray(env, sources).obj(),
      base::android::ToJavaArrayOfStrings(env, names).obj());
}

bool RegisterInvalidationController(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

} //  namespace invalidation
