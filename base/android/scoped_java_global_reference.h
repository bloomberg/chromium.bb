// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_SCOPED_JAVA_GLOBAL_REFERENCE_H_
#define BASE_ANDROID_SCOPED_JAVA_GLOBAL_REFERENCE_H_

#include <jni.h>
#include <stddef.h>

#include "base/android/scoped_java_reference.h"
#include "base/basictypes.h"

namespace base {
namespace android {

// Holds a global reference to a Java object. The global reference is scoped
// to the lifetime of this object. Note that since a JNI Env is only suitable
// for use on a single thread, objects of this class must be created, used and
// destroyed on the same thread.
template<typename T>
class ScopedJavaGlobalReference {
 public:
  // Holds a NULL reference.
  ScopedJavaGlobalReference()
      : env_(NULL),
        obj_(NULL) {
  }
  // Takes a new global reference to the Java object held by obj.
  explicit ScopedJavaGlobalReference(const ScopedJavaReference<T>& obj)
      : env_(obj.env()),
        obj_(static_cast<T>(obj.env()->NewGlobalRef(obj.obj()))) {
  }
  ~ScopedJavaGlobalReference() {
    Reset();
  }

  void Reset() {
    if (env_ && obj_)
      env_->DeleteGlobalRef(obj_);
    env_ = NULL;
    obj_ = NULL;
  }
  void Reset(const ScopedJavaGlobalReference& other) {
    Reset();
    env_ = other.env_;
    obj_ = other.env_ ? static_cast<T>(other.env_->NewGlobalRef(other.obj_)) :
        NULL;
  }

  T obj() const { return obj_; }

 private:
  JNIEnv* env_;
  T obj_;

  DISALLOW_COPY_AND_ASSIGN(ScopedJavaGlobalReference);
};

}  // namespace android
}  // namespace base

#endif  // BASE_ANDROID_SCOPED_JAVA_GLOBAL_REFERENCE_H_
