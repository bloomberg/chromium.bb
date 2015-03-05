// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/download_overwrite_infobar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/files/file_path.h"
#include "chrome/browser/android/download/download_overwrite_infobar_delegate.h"
#include "jni/DownloadOverwriteInfoBar_jni.h"

using chrome::android::DownloadOverwriteInfoBarDelegate;

// static
scoped_ptr<infobars::InfoBar> DownloadOverwriteInfoBar::CreateInfoBar(
    scoped_ptr<DownloadOverwriteInfoBarDelegate> delegate) {
  return make_scoped_ptr(new DownloadOverwriteInfoBar(delegate.Pass()));
}

DownloadOverwriteInfoBar::~DownloadOverwriteInfoBar() {
}

DownloadOverwriteInfoBar::DownloadOverwriteInfoBar(
    scoped_ptr<DownloadOverwriteInfoBarDelegate> delegate)
    : InfoBarAndroid(delegate.Pass()) {
}

base::android::ScopedJavaLocalRef<jobject>
DownloadOverwriteInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  DownloadOverwriteInfoBarDelegate* delegate = GetDelegate();

  ScopedJavaLocalRef<jstring> j_file_name =
      base::android::ConvertUTF8ToJavaString(env, delegate->GetFileName());
  ScopedJavaLocalRef<jstring> j_dir_name =
      base::android::ConvertUTF8ToJavaString(env, delegate->GetDirName());
  ScopedJavaLocalRef<jstring> j_dir_full_path =
      base::android::ConvertUTF8ToJavaString(env, delegate->GetDirFullPath());
  base::android::ScopedJavaLocalRef<jobject> java_infobar(
      Java_DownloadOverwriteInfoBar_createInfoBar(
          env, reinterpret_cast<intptr_t>(this), j_file_name.obj(),
          j_dir_name.obj(), j_dir_full_path.obj()));
  return java_infobar;
}

void DownloadOverwriteInfoBar::ProcessButton(int action,
                                             const std::string& action_value) {
  if (!owner())
    return;  // We're closing; don't call anything, it might access the owner.

  DownloadOverwriteInfoBarDelegate* delegate = GetDelegate();
  if (action == InfoBarAndroid::ACTION_OVERWRITE)
    delegate->OverwriteExistingFile();
  else if (action == InfoBarAndroid::ACTION_CREATE_NEW_FILE)
    delegate->CreateNewFile();
  else
    DCHECK(false);

  RemoveSelf();
}

DownloadOverwriteInfoBarDelegate* DownloadOverwriteInfoBar::GetDelegate() {
  return static_cast<DownloadOverwriteInfoBarDelegate*>(delegate());
}

// Native JNI methods ---------------------------------------------------------

bool RegisterDownloadOverwriteInfoBarDelegate(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
