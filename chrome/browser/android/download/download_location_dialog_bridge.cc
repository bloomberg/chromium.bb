// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/download/download_location_dialog_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/android/download/download_controller.h"
#include "chrome/browser/android/download/download_manager_service.h"
#include "content/public/browser/web_contents.h"
#include "jni/DownloadLocationDialogBridge_jni.h"

DownloadLocationDialogBridge::DownloadLocationDialogBridge()
    : is_dialog_showing_(false) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(env, Java_DownloadLocationDialogBridge_create(
                           env, reinterpret_cast<long>(this))
                           .obj());
  DCHECK(!java_obj_.is_null());
}

DownloadLocationDialogBridge::~DownloadLocationDialogBridge() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_DownloadLocationDialogBridge_destroy(env, java_obj_);
}

void DownloadLocationDialogBridge::ShowDialog(
    content::WebContents* web_contents,
    const base::FilePath& suggested_path,
    const DownloadTargetDeterminerDelegate::ConfirmationCallback& callback) {
  if (!web_contents)
    return;

  // If dialog is showing, run the callback to continue without confirmation.
  if (is_dialog_showing_) {
    if (!callback.is_null()) {
      callback.Run(DownloadConfirmationResult::CONTINUE_WITHOUT_CONFIRMATION,
                   suggested_path);
    }
    return;
  }

  is_dialog_showing_ = true;
  dialog_complete_callback_ = callback;

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_DownloadLocationDialogBridge_showDialog(
      env, java_obj_, web_contents->GetJavaWebContents(),
      base::android::ConvertUTF8ToJavaString(env,
                                             suggested_path.AsUTF8Unsafe()));
}

void DownloadLocationDialogBridge::OnComplete(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& returned_path) {
  std::string path_string(
      base::android::ConvertJavaStringToUTF8(env, returned_path));

  if (!dialog_complete_callback_.is_null()) {
    base::ResetAndReturn(&dialog_complete_callback_)
        .Run(DownloadConfirmationResult::CONFIRMED,
             base::FilePath(FILE_PATH_LITERAL(path_string)));
  }

  is_dialog_showing_ = false;
}

void DownloadLocationDialogBridge::OnCanceled(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  if (!dialog_complete_callback_.is_null()) {
    DownloadController::RecordDownloadCancelReason(
        DownloadController::CANCEL_REASON_USER_CANCELED);
    base::ResetAndReturn(&dialog_complete_callback_)
        .Run(DownloadConfirmationResult::CANCELED, base::FilePath());
  }

  is_dialog_showing_ = false;
}
