// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/base_jni_onload.h"

#include "base/android/jni_android.h"
#include "base/android/jni_onload_delegate.h"
#include "base/android/library_loader/library_loader_hooks.h"

namespace base {
namespace android {

namespace {

// The JNIOnLoadDelegate implementation in base.
class BaseJNIOnLoadDelegate : public JNIOnLoadDelegate {
 public:
  bool RegisterJNI(JNIEnv* env) override;
  bool Init() override;
};

bool BaseJNIOnLoadDelegate::RegisterJNI(JNIEnv* env) {
  return RegisterLibraryLoaderEntryHook(env);
}

bool BaseJNIOnLoadDelegate::Init() {
  return true;
}

}  // namespace


bool OnJNIOnLoad(JavaVM* vm,
                 std::vector<JNIOnLoadDelegate*>* delegates) {
  base::android::InitVM(vm);
  JNIEnv* env = base::android::AttachCurrentThread();

  BaseJNIOnLoadDelegate delegate;
  delegates->push_back(&delegate);
  bool ret = true;
  for (std::vector<JNIOnLoadDelegate*>::reverse_iterator i =
           delegates->rbegin(); i != delegates->rend(); ++i) {
    if (!(*i)->RegisterJNI(env)) {
      ret = false;
      break;
    }
  }

  if (ret) {
    for (std::vector<JNIOnLoadDelegate*>::reverse_iterator i =
             delegates->rbegin(); i != delegates->rend(); ++i) {
      if (!(*i)->Init()) {
        ret = false;
        break;
      }
    }
  }
  delegates->pop_back();
  return ret;
}

}  // namespace android
}  // namespace base
