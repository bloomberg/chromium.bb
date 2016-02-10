// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/download/download_manager_service.h"

#include "base/android/jni_string.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/android/download_controller_android.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item.h"
#include "jni/DownloadManagerService_jni.h"

using base::android::JavaParamRef;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;

namespace {
// The retry interval when resuming/canceling a download. This is needed because
// when the browser process is launched, we have to wait until the download
// history get loaded before a download can be resumed/cancelled. However,
// we don't want to retry after a long period of time as the same download Id
// can be reused later.
const int kRetryIntervalInMilliseconds = 3000;
}

// static
bool DownloadManagerService::RegisterDownloadManagerService(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static jlong Init(JNIEnv* env, const JavaParamRef<jobject>& jobj) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  content::DownloadManager* manager =
      content::BrowserContext::GetDownloadManager(profile);
  DownloadManagerService* service =
      new DownloadManagerService(env, jobj, manager);
  return reinterpret_cast<intptr_t>(service);
}

DownloadManagerService::DownloadManagerService(
    JNIEnv* env,
    jobject obj,
    content::DownloadManager* manager)
    : java_ref_(env, obj), manager_(manager) {
  manager_->AddObserver(this);
}

DownloadManagerService::~DownloadManagerService() {
  if (manager_)
    manager_->RemoveObserver(this);
}

void DownloadManagerService::ResumeDownload(JNIEnv* env,
                                            jobject obj,
                                            uint32_t download_id,
                                            jstring fileName) {
  ResumeDownloadInternal(download_id, ConvertJavaStringToUTF8(env, fileName),
                         true);
}

void DownloadManagerService::CancelDownload(JNIEnv* env,
                                            jobject obj,
                                            uint32_t download_id) {
  CancelDownloadInternal(download_id, true);
}

void DownloadManagerService::PauseDownload(JNIEnv* env,
                                            jobject obj,
                                            uint32_t download_id) {
  content::DownloadItem* item = manager_->GetDownload(download_id);
  if (item)
    item->Pause();
}

void DownloadManagerService::ManagerGoingDown(
    content::DownloadManager* manager) {
  manager_ = nullptr;
}

void DownloadManagerService::ResumeDownloadItem(content::DownloadItem* item,
                                                const std::string& fileName) {
  if (!item->CanResume()) {
    OnResumptionFailed(item->GetId(), fileName);
    return;
  }
  item->AddObserver(content::DownloadControllerAndroid::Get());
  item->Resume();
  if (!resume_callback_for_testing_.is_null())
    resume_callback_for_testing_.Run(true);
}

void DownloadManagerService::ResumeDownloadInternal(uint32_t download_id,
                                                    const std::string& fileName,
                                                    bool retry) {
  if (!manager_) {
    OnResumptionFailed(download_id, fileName);
    return;
  }
  content::DownloadItem* item = manager_->GetDownload(download_id);
  if (item) {
    ResumeDownloadItem(item, fileName);
    return;
  }
  if (!retry) {
    OnResumptionFailed(download_id, fileName);
    return;
  }
  // Post a delayed task to wait for the download history to load the download
  // item. If the download item is not loaded when the delayed task runs, show
  // an download failed notification. Alternatively, we can have the
  // DownloadManager inform us when a download item is created. However, there
  // is no guarantee when the download item will be created, since the newly
  // created item might not be loaded from download history. So user might wait
  // indefinitely to see the failed notification. See http://crbug.com/577893.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DownloadManagerService::ResumeDownloadInternal,
                 base::Unretained(this), download_id, fileName, false),
      base::TimeDelta::FromMilliseconds(kRetryIntervalInMilliseconds));
}

void DownloadManagerService::CancelDownloadInternal(uint32_t download_id,
                                                    bool retry) {
  if (!manager_)
    return;
  content::DownloadItem* item = manager_->GetDownload(download_id);
  if (item) {
    item->Cancel(true);
    return;
  }
  if (retry) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE, base::Bind(&DownloadManagerService::CancelDownloadInternal,
                              base::Unretained(this), download_id, false),
        base::TimeDelta::FromMilliseconds(kRetryIntervalInMilliseconds));
  }
}

void DownloadManagerService::OnResumptionFailed(uint32_t download_id,
                                                const std::string& fileName) {
  if (!java_ref_.is_null()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_DownloadManagerService_onResumptionFailed(
        env, java_ref_.obj(), download_id,
        ConvertUTF8ToJavaString(env, fileName).obj());
  }
  if (!resume_callback_for_testing_.is_null())
    resume_callback_for_testing_.Run(false);
}
