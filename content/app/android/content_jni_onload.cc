// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/app/content_jni_onload.h"

#include <vector>

#include "base/android/base_jni_onload.h"
#include "base/android/jni_onload_delegate.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "content/app/android/library_loader_hooks.h"
#include "content/public/app/content_main.h"

namespace content {
namespace android {

namespace {

class ContentJNIOnLoadDelegate
    : public base::android::JNIOnLoadDelegate {
 public:
  bool RegisterJNI(JNIEnv* env) override;
  bool Init() override;
};

bool ContentJNIOnLoadDelegate::RegisterJNI(JNIEnv* env) {
  return content::EnsureJniRegistered(env);
}

bool ContentJNIOnLoadDelegate::Init() {
  base::android::SetLibraryLoadedHook(&content::LibraryLoaded);
  return true;
}

}  // namespace


bool OnJNIOnLoad(JavaVM* vm,
                 base::android::JNIOnLoadDelegate* delegate) {
  std::vector<base::android::JNIOnLoadDelegate*> delegates;
  ContentJNIOnLoadDelegate content_delegate;
  delegates.push_back(delegate);
  delegates.push_back(&content_delegate);
  return base::android::OnJNIOnLoad(vm, &delegates);
}

}  // namespace android
}  // namespace content
