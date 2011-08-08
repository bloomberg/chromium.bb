// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_SCOPED_JAVA_REFERENCE_H_
#define BASE_ANDROID_SCOPED_JAVA_REFERENCE_H_

#include <jni.h>
#include <stddef.h>

namespace base {
namespace android {

// Holds a local reference to a Java object. The local reference is scoped
// to the lifetime of this object. Note that since a JNI Env is only suitable
// for use on a single thread, objects of this class must be created, used and
// destroyed on the same thread.
template<typename T>
class ScopedJavaReference {
 public:
  // Holds a NULL reference.
  ScopedJavaReference()
      : env_(NULL),
        obj_(NULL) {
  }
  // Assumes that obj is a local reference to a Java object and takes ownership
  // of this local reference.
  ScopedJavaReference(JNIEnv* env, T obj)
      : env_(env),
        obj_(obj) {
  }
  // Takes a new local reference to the Java object held by other.
  ScopedJavaReference(const ScopedJavaReference& other)
      : env_(other.env_),
        obj_(other.obj_ ? static_cast<T>(other.env_->NewLocalRef(other.obj_)) :
            NULL) {
  }
  ~ScopedJavaReference() {
    if (obj_)
      env_->DeleteLocalRef(obj_);
  }

  void operator=(const ScopedJavaReference& other) {
    if (obj_)
      env_->DeleteLocalRef(obj_);
    env_ = other.env_;
    obj_ = other.obj_ ? static_cast<T>(env_->NewLocalRef(other.obj_)) : NULL;
  }

  JNIEnv* env() const { return env_; }
  T obj() const { return obj_; }

  // Releases the local reference to the caller. The caller *must* delete the
  // local reference when it is done with it.
  T Release() {
    jobject obj = obj_;
    obj_ = NULL;
    return obj;
  }

 private:
  JNIEnv* env_;
  T obj_;
};

}  // namespace android
}  // namespace base

#endif  // BASE_ANDROID_SCOPED_JAVA_REFERENCE_H_
