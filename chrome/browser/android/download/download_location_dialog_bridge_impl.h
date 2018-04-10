// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_LOCATION_DIALOG_BRIDGE_IMPL_H_
#define CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_LOCATION_DIALOG_BRIDGE_IMPL_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "chrome/browser/android/download/download_location_dialog_bridge.h"
#include "chrome/browser/download/download_confirmation_result.h"
#include "chrome/browser/download/download_location_dialog_type.h"
#include "chrome/browser/download/download_target_determiner_delegate.h"
#include "ui/gfx/native_widget_types.h"

class DownloadLocationDialogBridgeImpl : public DownloadLocationDialogBridge {
 public:
  DownloadLocationDialogBridgeImpl();
  ~DownloadLocationDialogBridgeImpl() override;

  // DownloadLocationDialogBridge implementation.
  void ShowDialog(gfx::NativeWindow native_window,
                  DownloadLocationDialogType dialog_type,
                  const base::FilePath& suggested_path,
                  const DownloadTargetDeterminerDelegate::ConfirmationCallback&
                      callback) override;

  void OnComplete(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& returned_path) override;

  void OnCanceled(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& obj) override;

 private:
  jboolean is_dialog_showing_;
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
  DownloadTargetDeterminerDelegate::ConfirmationCallback
      dialog_complete_callback_;

  DISALLOW_COPY_AND_ASSIGN(DownloadLocationDialogBridgeImpl);
};

#endif  // CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_LOCATION_DIALOG_BRIDGE_IMPL_H_
