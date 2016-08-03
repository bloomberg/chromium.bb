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

namespace {

bool ShouldShowDownloadItem(content::DownloadItem* item) {
  return !item->IsTemporary() &&
      !item->GetFileNameToReportUser().empty() &&
      !item->GetTargetFilePath().empty() &&
      item->GetState() == content::DownloadItem::COMPLETE;
}

}  // namespace

// static
bool DownloadManagerService::RegisterDownloadManagerService(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
void DownloadManagerService::OnDownloadCanceled(
    content::DownloadItem* download,
    DownloadController::DownloadCancelReason reason) {
  bool has_no_external_storage =
      (reason == DownloadController::CANCEL_REASON_NO_EXTERNAL_STORAGE);
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jname =
      ConvertUTF8ToJavaString(env, download->GetURL().ExtractFileName());
  Java_DownloadManagerService_onDownloadItemCanceled(
      env, jname.obj(), has_no_external_storage);
  DownloadController::RecordDownloadCancelReason(reason);
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
    const JavaParamRef<jstring>& jdownload_guid,
    bool is_off_the_record) {
  std::string download_guid = ConvertJavaStringToUTF8(env, jdownload_guid);
  if (is_history_query_complete_ || is_off_the_record)
    ResumeDownloadInternal(download_guid, is_off_the_record);
  else
    EnqueueDownloadAction(download_guid, RESUME);
}

void DownloadManagerService::PauseDownload(
    JNIEnv* env,
    jobject obj,
    const JavaParamRef<jstring>& jdownload_guid,
    bool is_off_the_record) {
  std::string download_guid = ConvertJavaStringToUTF8(env, jdownload_guid);
  if (is_history_query_complete_ || is_off_the_record)
    PauseDownloadInternal(download_guid, is_off_the_record);
  else
    EnqueueDownloadAction(download_guid, PAUSE);
}

void DownloadManagerService::GetAllDownloads(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj) {
  if (is_history_query_complete_)
    GetAllDownloadsInternal();
  else
    EnqueueDownloadAction(std::string(), INITIALIZE_UI);
}

void DownloadManagerService::GetDownloadInfoFor(
    JNIEnv* env,
    jobject obj,
    const JavaParamRef<jstring>& jdownload_guid,
    bool is_off_the_record) {
  // The UI shouldn't be able to request any info about a specific item until it
  // has been initialized with the list of existing downloads.
  DCHECK(is_history_query_complete_ || is_off_the_record);

  content::DownloadManager* manager = GetDownloadManager(is_off_the_record);
  if (java_ref_.is_null() || !manager)
    return;

  // Search through the downloads for the entry with the given GUID.
  content::DownloadManager::DownloadVector all_items;
  manager->GetAllDownloads(&all_items);
  std::string download_guid = ConvertJavaStringToUTF8(env, jdownload_guid);
  for (auto item : all_items) {
    if (item->GetGuid() != download_guid)
      continue;

    Java_DownloadManagerService_onDownloadItemUpdated(
        env,
        java_ref_.obj(),
        ConvertUTF8ToJavaString(env, item->GetGuid()).obj(),
        ConvertUTF8ToJavaString(
            env, item->GetFileNameToReportUser().value()).obj(),
        ConvertUTF8ToJavaString(
            env, item->GetTargetFilePath().value()).obj(),
        ConvertUTF8ToJavaString(env, item->GetTabUrl().spec()).obj(),
        ConvertUTF8ToJavaString(env, item->GetMimeType()).obj(),
        item->GetStartTime().ToJavaTime(),
        item->GetTotalBytes());
    break;
  }
}

void DownloadManagerService::GetAllDownloadsInternal() {
  content::DownloadManager* manager = GetDownloadManager(false);
  if (java_ref_.is_null() || !manager)
    return;

  content::DownloadManager::DownloadVector all_items;
  manager->GetAllDownloads(&all_items);

  // Create a Java array of all of the visible DownloadItems.
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_download_item_list =
      Java_DownloadManagerService_createDownloadItemList(env, java_ref_.obj());

  for (size_t i = 0; i < all_items.size(); i++) {
    content::DownloadItem* item = all_items[i];
    if (!ShouldShowDownloadItem(item))
      continue;

    Java_DownloadManagerService_addDownloadItemToList(
        env,
        java_ref_.obj(),
        j_download_item_list.obj(),
        ConvertUTF8ToJavaString(env, item->GetGuid()).obj(),
        ConvertUTF8ToJavaString(
            env, item->GetFileNameToReportUser().value()).obj(),
        ConvertUTF8ToJavaString(
            env, item->GetTargetFilePath().value()).obj(),
        ConvertUTF8ToJavaString(env, item->GetTabUrl().spec()).obj(),
        ConvertUTF8ToJavaString(env, item->GetMimeType()).obj(),
        item->GetStartTime().ToJavaTime(),
        item->GetTotalBytes());
  }

  Java_DownloadManagerService_onAllDownloadsRetrieved(
      env, java_ref_.obj(), j_download_item_list.obj());
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
  if (is_history_query_complete_ || is_off_the_record)
    CancelDownloadInternal(download_guid, is_off_the_record);
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
        ResumeDownloadInternal(download_guid, false);
        break;
      case PAUSE:
        PauseDownloadInternal(download_guid, false);
        break;
      case CANCEL:
        CancelDownloadInternal(download_guid, false);
        break;
      case INITIALIZE_UI:
        GetAllDownloadsInternal();
        break;
      default:
        NOTREACHED();
        break;
    }
  }
  pending_actions_.clear();
}

void DownloadManagerService::ResumeDownloadInternal(
    const std::string& download_guid, bool is_off_the_record) {
  content::DownloadManager* manager = GetDownloadManager(is_off_the_record);
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
    const std::string& download_guid, bool is_off_the_record) {
  content::DownloadManager* manager = GetDownloadManager(is_off_the_record);
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
    case INITIALIZE_UI:
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
