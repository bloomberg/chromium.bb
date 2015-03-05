// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/download/android_download_manager_overwrite_infobar_delegate.h"

#include "base/android/jni_string.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/android/download/chrome_download_delegate.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/android/infobars/download_overwrite_infobar.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

using base::android::ScopedJavaLocalRef;

namespace chrome {
namespace android {

AndroidDownloadManagerOverwriteInfoBarDelegate::
    ~AndroidDownloadManagerOverwriteInfoBarDelegate() {
}

// static
void AndroidDownloadManagerOverwriteInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    const std::string& file_name,
    const std::string& dir_name,
    const std::string& dir_full_path,
    jobject chrome_download_delegate,
    jobject download_info) {
  infobar_service->AddInfoBar(DownloadOverwriteInfoBar::CreateInfoBar(
      make_scoped_ptr(new AndroidDownloadManagerOverwriteInfoBarDelegate(
          file_name, dir_name, dir_full_path, chrome_download_delegate,
          download_info))));
}

AndroidDownloadManagerOverwriteInfoBarDelegate::
    AndroidDownloadManagerOverwriteInfoBarDelegate(
        const std::string& file_name,
        const std::string& dir_name,
        const std::string& dir_full_path,
        jobject chrome_download_delegate,
        jobject download_info)
    : file_name_(file_name),
      dir_name_(dir_name),
      dir_full_path_(dir_full_path) {
  JNIEnv* env = base::android::AttachCurrentThread();
  chrome_download_delegate_.Reset(env, chrome_download_delegate);
  download_info_.Reset(env, download_info);
}

void AndroidDownloadManagerOverwriteInfoBarDelegate::OverwriteExistingFile() {
  ChromeDownloadDelegate::EnqueueDownloadManagerRequest(
      chrome_download_delegate_.obj(), true, download_info_.obj());
}

void AndroidDownloadManagerOverwriteInfoBarDelegate::CreateNewFile() {
  ChromeDownloadDelegate::EnqueueDownloadManagerRequest(
      chrome_download_delegate_.obj(), false, download_info_.obj());
}

std::string AndroidDownloadManagerOverwriteInfoBarDelegate::GetFileName()
    const {
  return file_name_;
}

std::string AndroidDownloadManagerOverwriteInfoBarDelegate::GetDirName() const {
  return dir_name_;
}

std::string AndroidDownloadManagerOverwriteInfoBarDelegate::GetDirFullPath()
    const {
  return dir_full_path_;
}

}  // namespace android
}  // namespace chrome
