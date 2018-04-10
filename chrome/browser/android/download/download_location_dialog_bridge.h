// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_LOCATION_DIALOG_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_LOCATION_DIALOG_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/download/download_location_dialog_type.h"
#include "chrome/browser/download/download_target_determiner_delegate.h"
#include "ui/gfx/native_widget_types.h"

class DownloadLocationDialogBridge {
 public:
  virtual ~DownloadLocationDialogBridge() = default;

  virtual void ShowDialog(
      gfx::NativeWindow native_window,
      DownloadLocationDialogType dialog_type,
      const base::FilePath& suggested_path,
      const DownloadTargetDeterminerDelegate::ConfirmationCallback&
          callback) = 0;

  virtual void OnComplete(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& returned_path) = 0;

  virtual void OnCanceled(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj) = 0;
};

#endif  // CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_LOCATION_DIALOG_BRIDGE_H_
