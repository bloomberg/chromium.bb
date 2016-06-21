// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/download/download_manager_service.h"

#include "base/android/jni_string.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/android/download/download_controller.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item.h"
#include "jni/DownloadManagerService_jni.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::JavaParamRef;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;

// static
bool DownloadManagerService::RegisterDownloadManagerService(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static jlong Init(JNIEnv* env, const JavaParamRef<jobject>& jobj) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  DownloadManagerService* service =
      new DownloadManagerService(env, jobj);
  DownloadService* download_service =
      DownloadServiceFactory::GetForBrowserContext(profile);
  DownloadHistory* history = download_service->GetDownloadHistory();
  if (history)
    history->AddObserver(service);
  return reinterpret_cast<intptr_t>(service);
}

DownloadManagerService::DownloadManagerService(
    JNIEnv* env,
    jobject obj)
    : java_ref_(env, obj),
      is_history_query_complete_(false) {
  DownloadControllerBase::Get()->SetDefaultDownloadFileName(
      l10n_util::GetStringUTF8(IDS_DEFAULT_DOWNLOAD_FILENAME));
}

DownloadManagerService::~DownloadManagerService() {}

void DownloadManagerService::ResumeDownload(
    JNIEnv* env,
    jobject obj,
    const JavaParamRef<jstring>& jdownload_guid) {
  std::string download_guid = ConvertJavaStringToUTF8(env, jdownload_guid);
  if (is_history_query_complete_)
    ResumeDownloadInternal(download_guid);
  else
    EnqueueDownloadAction(download_guid, RESUME);
}

void DownloadManagerService::PauseDownload(
    JNIEnv* env,
    jobject obj,
    const JavaParamRef<jstring>& jdownload_guid) {
  std::string download_guid = ConvertJavaStringToUTF8(env, jdownload_guid);
  if (is_history_query_complete_)
    PauseDownloadInternal(download_guid);
  else
    EnqueueDownloadAction(download_guid, PAUSE);
}

void DownloadManagerService::CancelDownload(
    JNIEnv* env,
    jobject obj,
    const JavaParamRef<jstring>& jdownload_guid,
    bool is_off_the_record,
    bool is_notification_dismissed) {
  std::string download_guid = ConvertJavaStringToUTF8(env, jdownload_guid);

  DownloadController::RecordDownloadCancelReason(
      is_notification_dismissed ?
          DownloadController::CANCEL_REASON_NOTIFICATION_DISMISSED :
          DownloadController::CANCEL_REASON_ACTION_BUTTON);
  // Incognito download can only be cancelled in the same browser session, no
  // need to wait for download history.
  if (is_off_the_record) {
    CancelDownloadInternal(download_guid, true);
    return;
  }

  if (is_history_query_complete_)
    CancelDownloadInternal(download_guid, false);
  else
    EnqueueDownloadAction(download_guid, CANCEL);
}

void DownloadManagerService::OnHistoryQueryComplete() {
  is_history_query_complete_ = true;
  for (auto iter = pending_actions_.begin(); iter != pending_actions_.end();
       ++iter) {
    DownloadAction action = iter->second;
    std::string download_guid = iter->first;
    switch (action) {
      case RESUME:
        ResumeDownloadInternal(download_guid);
        break;
      case PAUSE:
        PauseDownloadInternal(download_guid);
        break;
      case CANCEL:
        CancelDownloadInternal(download_guid, false);
        break;
      default:
        NOTREACHED();
        break;
    }
  }
  pending_actions_.clear();
}

void DownloadManagerService::ResumeDownloadInternal(
    const std::string& download_guid) {
  content::DownloadManager* manager = GetDownloadManager(false);
  if (!manager) {
    OnResumptionFailed(download_guid);
    return;
  }
  content::DownloadItem* item = manager->GetDownloadByGuid(download_guid);
  if (!item) {
    OnResumptionFailed(download_guid);
    return;
  }
  if (!item->CanResume()) {
    OnResumptionFailed(download_guid);
    return;
  }
  item->AddObserver(DownloadControllerBase::Get());
  item->Resume();
  if (!resume_callback_for_testing_.is_null())
    resume_callback_for_testing_.Run(true);
}

void DownloadManagerService::CancelDownloadInternal(
    const std::string& download_guid, bool is_off_the_record) {
  content::DownloadManager* manager = GetDownloadManager(is_off_the_record);
  if (!manager)
    return;
  content::DownloadItem* item = manager->GetDownloadByGuid(download_guid);
  if (item) {
    item->Cancel(true);
    item->RemoveObserver(DownloadControllerBase::Get());
  }
}

void DownloadManagerService::PauseDownloadInternal(
    const std::string& download_guid) {
  content::DownloadManager* manager = GetDownloadManager(false);
  if (!manager)
    return;
  content::DownloadItem* item = manager->GetDownloadByGuid(download_guid);
  if (item) {
    item->Pause();
    item->RemoveObserver(DownloadControllerBase::Get());
  }
}

void DownloadManagerService::EnqueueDownloadAction(
    const std::string& download_guid,
    DownloadAction action) {
  auto iter = pending_actions_.find(download_guid);
  if (iter == pending_actions_.end()) {
    pending_actions_[download_guid] = action;
    return;
  }
  switch (action) {
    case RESUME:
      if (iter->second == PAUSE)
        iter->second = action;
      break;
    case PAUSE:
      if (iter->second == RESUME)
        iter->second = action;
      break;
    case CANCEL:
      iter->second = action;
      break;
    default:
      NOTREACHED();
      break;
  }
}

void DownloadManagerService::OnResumptionFailed(
    const std::string& download_guid) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&DownloadManagerService::OnResumptionFailedInternal,
                            base::Unretained(this), download_guid));
  DownloadController::RecordDownloadCancelReason(
      DownloadController::CANCEL_REASON_NOT_CANCELED);
}

void DownloadManagerService::OnResumptionFailedInternal(
    const std::string& download_guid) {
  if (!java_ref_.is_null()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_DownloadManagerService_onResumptionFailed(
        env, java_ref_.obj(),
        ConvertUTF8ToJavaString(env, download_guid).obj());
  }
  if (!resume_callback_for_testing_.is_null())
    resume_callback_for_testing_.Run(false);
}

content::DownloadManager* DownloadManagerService::GetDownloadManager(
    bool is_off_the_record) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (is_off_the_record)
    profile = profile->GetOffTheRecordProfile();
  return content::BrowserContext::GetDownloadManager(profile);
}
