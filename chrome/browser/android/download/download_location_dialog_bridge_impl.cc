// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/download/download_location_dialog_bridge_impl.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/android/download/download_controller.h"
#include "chrome/browser/android/download/download_manager_service.h"
#include "jni/DownloadLocationDialogBridge_jni.h"
#include "ui/android/window_android.h"

DownloadLocationDialogBridgeImpl::DownloadLocationDialogBridgeImpl()
    : is_dialog_showing_(false) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(env, Java_DownloadLocationDialogBridge_create(
                           env, reinterpret_cast<long>(this))
                           .obj());
  DCHECK(!java_obj_.is_null());
}

DownloadLocationDialogBridgeImpl::~DownloadLocationDialogBridgeImpl() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_DownloadLocationDialogBridge_destroy(env, java_obj_);
}

void DownloadLocationDialogBridgeImpl::ShowDialog(
    gfx::NativeWindow native_window,
    DownloadLocationDialogType dialog_type,
    const base::FilePath& suggested_path,
    const DownloadTargetDeterminerDelegate::ConfirmationCallback& callback) {
  if (!native_window)
    return;

  // This shouldn't happen, but if it does, cancel download.
  if (dialog_type == DownloadLocationDialogType::NO_DIALOG) {
    NOTREACHED();
    callback.Run(DownloadConfirmationResult::CANCELED, base::FilePath());
  }

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
      env, java_obj_, native_window->GetJavaObject(),
      static_cast<int>(dialog_type),
      base::android::ConvertUTF8ToJavaString(env,
                                             suggested_path.AsUTF8Unsafe()));
}

void DownloadLocationDialogBridgeImpl::OnComplete(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& returned_path) {
  std::string path_string(
      base::android::ConvertJavaStringToUTF8(env, returned_path));

  if (!dialog_complete_callback_.is_null()) {
    base::ResetAndReturn(&dialog_complete_callback_)
        .Run(DownloadConfirmationResult::CONFIRMED_WITH_DIALOG,
             base::FilePath(FILE_PATH_LITERAL(path_string)));
  }

  is_dialog_showing_ = false;
}

void DownloadLocationDialogBridgeImpl::OnCanceled(
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
