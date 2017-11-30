// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/download/download_manager_service.h"

#include <memory>

#include "base/android/jni_string.h"
#include "base/location.h"
#include "base/metrics/field_trial_params.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/android/download/download_controller.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item.h"
#include "jni/DownloadInfo_jni.h"
#include "jni/DownloadItem_jni.h"
#include "jni/DownloadManagerService_jni.h"
#include "third_party/WebKit/common/mime_util/mime_util.h"

using base::android::JavaParamRef;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace {

// The remaining time for a download item if it cannot be calculated.
constexpr int64_t kUnknownRemainingTime = -1;

// Finch flag for controlling auto resumption limit.
int kDefaultAutoResumptionLimit = 5;
const char kAutoResumptionLimitParamName[] = "AutoResumptionLimit";

bool ShouldShowDownloadItem(content::DownloadItem* item) {
  return !item->IsTemporary() && !item->IsTransient();
}

ScopedJavaLocalRef<jobject> JNI_DownloadManagerService_CreateJavaDownloadItem(
    JNIEnv* env,
    content::DownloadItem* item) {
  DCHECK(!item->IsTransient());
  return Java_DownloadItem_createDownloadItem(
      env, DownloadManagerService::CreateJavaDownloadInfo(env, item),
      item->GetStartTime().ToJavaTime(), item->GetFileExternallyRemoved());
}

}  // namespace

// static
void DownloadManagerService::OnDownloadCanceled(
    content::DownloadItem* download,
    DownloadController::DownloadCancelReason reason) {
  if (download->IsTransient()) {
    LOG(WARNING) << "Transient download should not have user interaction!";
    return;
  }

  // Inform the user in Java UI about file writing failures.
  bool has_no_external_storage =
      (reason == DownloadController::CANCEL_REASON_NO_EXTERNAL_STORAGE);
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jname =
      ConvertUTF8ToJavaString(env, download->GetURL().ExtractFileName());
  Java_DownloadManagerService_onDownloadItemCanceled(env, jname,
                                                     has_no_external_storage);
  DownloadController::RecordDownloadCancelReason(reason);
}

// static
DownloadManagerService* DownloadManagerService::GetInstance() {
  return base::Singleton<DownloadManagerService>::get();
}

// static
ScopedJavaLocalRef<jobject> DownloadManagerService::CreateJavaDownloadInfo(
    JNIEnv* env, content::DownloadItem* item) {
  bool user_initiated =
      (item->GetTransitionType() & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR) ||
      PageTransitionCoreTypeIs(item->GetTransitionType(),
                               ui::PAGE_TRANSITION_TYPED) ||
      PageTransitionCoreTypeIs(item->GetTransitionType(),
                               ui::PAGE_TRANSITION_AUTO_BOOKMARK) ||
      PageTransitionCoreTypeIs(item->GetTransitionType(),
                               ui::PAGE_TRANSITION_GENERATED) ||
      PageTransitionCoreTypeIs(item->GetTransitionType(),
                               ui::PAGE_TRANSITION_RELOAD) ||
      PageTransitionCoreTypeIs(item->GetTransitionType(),
                               ui::PAGE_TRANSITION_KEYWORD);
  bool has_user_gesture = item->HasUserGesture() || user_initiated;

  base::TimeDelta time_delta;
  bool time_remaining_known = item->TimeRemaining(&time_delta);
  std::string original_url = item->GetOriginalUrl().SchemeIs(url::kDataScheme)
      ? std::string() : item->GetOriginalUrl().spec();
  return Java_DownloadInfo_createDownloadInfo(
      env, ConvertUTF8ToJavaString(env, item->GetGuid()),
      ConvertUTF8ToJavaString(env, item->GetFileNameToReportUser().value()),
      ConvertUTF8ToJavaString(env, item->GetTargetFilePath().value()),
      ConvertUTF8ToJavaString(env, item->GetTabUrl().spec()),
      ConvertUTF8ToJavaString(env, item->GetMimeType()),
      item->GetReceivedBytes(), item->GetBrowserContext()->IsOffTheRecord(),
      item->GetState(), item->PercentComplete(), item->IsPaused(),
      has_user_gesture, item->CanResume(),
      ConvertUTF8ToJavaString(env, original_url),
      ConvertUTF8ToJavaString(env, item->GetReferrerUrl().spec()),
      time_remaining_known ? time_delta.InMilliseconds()
                           : kUnknownRemainingTime,
      item->GetLastAccessTime().ToJavaTime());
}

static jlong JNI_DownloadManagerService_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  DownloadManagerService* service = DownloadManagerService::GetInstance();
  service->Init(env, jobj);
  DownloadCoreService* download_core_service =
      DownloadCoreServiceFactory::GetForBrowserContext(profile);
  DownloadHistory* history = download_core_service->GetDownloadHistory();
  if (history)
    history->AddObserver(service);
  return reinterpret_cast<intptr_t>(service);
}

DownloadManagerService::DownloadManagerService()
    : is_history_query_complete_(false),
      pending_get_downloads_actions_(NONE) {
}

DownloadManagerService::~DownloadManagerService() {}

void DownloadManagerService::Init(
    JNIEnv* env,
    jobject obj) {
  java_ref_.Reset(env, obj);
}

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

void DownloadManagerService::RemoveDownload(
    JNIEnv* env,
    jobject obj,
    const JavaParamRef<jstring>& jdownload_guid,
    bool is_off_the_record) {
  std::string download_guid = ConvertJavaStringToUTF8(env, jdownload_guid);
  if (is_history_query_complete_ || is_off_the_record)
    RemoveDownloadInternal(download_guid, is_off_the_record);
  else
    EnqueueDownloadAction(download_guid, REMOVE);
}

void DownloadManagerService::GetAllDownloads(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj,
                                             bool is_off_the_record) {
  if (is_history_query_complete_)
    GetAllDownloadsInternal(is_off_the_record);
  else if (is_off_the_record)
    pending_get_downloads_actions_ |= OFF_THE_RECORD;
  else
    pending_get_downloads_actions_ |= REGULAR;
}

void DownloadManagerService::GetAllDownloadsInternal(bool is_off_the_record) {
  content::DownloadManager* manager = GetDownloadManager(is_off_the_record);
  if (java_ref_.is_null() || !manager)
    return;

  content::DownloadManager::DownloadVector all_items;
  manager->GetAllDownloads(&all_items);

  // Create a Java array of all of the visible DownloadItems.
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_download_item_list =
      Java_DownloadManagerService_createDownloadItemList(env, java_ref_);

  for (size_t i = 0; i < all_items.size(); i++) {
    content::DownloadItem* item = all_items[i];
    if (!ShouldShowDownloadItem(item))
      continue;

    ScopedJavaLocalRef<jobject> j_item =
        JNI_DownloadManagerService_CreateJavaDownloadItem(env, item);
    Java_DownloadManagerService_addDownloadItemToList(
        env, java_ref_, j_download_item_list, j_item);
  }

  Java_DownloadManagerService_onAllDownloadsRetrieved(
      env, java_ref_, j_download_item_list, is_off_the_record);
}

void DownloadManagerService::CheckForExternallyRemovedDownloads(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    bool is_off_the_record) {
  // Once the history query is complete, download_history.cc will check for the
  // removal of history files. If the history query is not yet complete, ignore
  // requests to check for externally removed downloads.
  if (!is_history_query_complete_)
    return;

  content::DownloadManager* manager = GetDownloadManager(is_off_the_record);
  if (!manager)
    return;
  manager->CheckForHistoryFilesRemoval();
}

void DownloadManagerService::UpdateLastAccessTime(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& jdownload_guid,
    bool is_off_the_record) {
  std::string download_guid = ConvertJavaStringToUTF8(env, jdownload_guid);
  content::DownloadManager* manager = GetDownloadManager(is_off_the_record);
  if (!manager)
    return;

  content::DownloadItem* item = manager->GetDownloadByGuid(download_guid);
  if (item)
    item->SetLastAccessTime(base::Time::Now());
}

void DownloadManagerService::CancelDownload(
    JNIEnv* env,
    jobject obj,
    const JavaParamRef<jstring>& jdownload_guid,
    bool is_off_the_record) {
  std::string download_guid = ConvertJavaStringToUTF8(env, jdownload_guid);
  DownloadController::RecordDownloadCancelReason(
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
      default:
        NOTREACHED();
        break;
    }
  }
  pending_actions_.clear();

  // Respond to any requests to get all downloads.
  if (pending_get_downloads_actions_ & REGULAR)
    GetAllDownloadsInternal(false);
  if (pending_get_downloads_actions_ & OFF_THE_RECORD)
    GetAllDownloadsInternal(true);
}

void DownloadManagerService::OnDownloadCreated(
    content::DownloadManager* manager, content::DownloadItem* item) {
  if (item->IsTransient())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_item =
      JNI_DownloadManagerService_CreateJavaDownloadItem(env, item);
  Java_DownloadManagerService_onDownloadItemCreated(env, java_ref_, j_item);
}

void DownloadManagerService::OnDownloadUpdated(
    content::DownloadManager* manager, content::DownloadItem* item) {
  if (java_ref_.is_null())
    return;

  if (item->IsTemporary() || item->IsTransient())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_item =
      JNI_DownloadManagerService_CreateJavaDownloadItem(env, item);
  Java_DownloadManagerService_onDownloadItemUpdated(env, java_ref_, j_item);
}

void DownloadManagerService::OnDownloadRemoved(
    content::DownloadManager* manager, content::DownloadItem* item) {
  if (java_ref_.is_null() || item->IsTransient())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_DownloadManagerService_onDownloadItemRemoved(
      env, java_ref_, ConvertUTF8ToJavaString(env, item->GetGuid()),
      item->GetBrowserContext()->IsOffTheRecord());
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
  DownloadControllerBase::Get()->AboutToResumeDownload(item);
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
    // Remove the observer first to avoid item->Cancel() causing re-entrance
    // issue.
    item->RemoveObserver(DownloadControllerBase::Get());
    item->Cancel(true);
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

void DownloadManagerService::RemoveDownloadInternal(
    const std::string& download_guid, bool is_off_the_record) {
  content::DownloadManager* manager = GetDownloadManager(is_off_the_record);
  if (!manager)
    return;
  content::DownloadItem* item = manager->GetDownloadByGuid(download_guid);
  if (item)
    item->Remove();
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
    case REMOVE:
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
        env, java_ref_, ConvertUTF8ToJavaString(env, download_guid));
  }
  if (!resume_callback_for_testing_.is_null())
    resume_callback_for_testing_.Run(false);
}

content::DownloadManager* DownloadManagerService::GetDownloadManager(
    bool is_off_the_record) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (is_off_the_record)
    profile = profile->GetOffTheRecordProfile();

  auto& notifier =
      is_off_the_record ? off_the_record_notifier_ : original_notifier_;
  content::DownloadManager* manager =
      content::BrowserContext::GetDownloadManager(profile);
  if (!manager) {
    notifier.reset();
    return nullptr;
  }

  // Update notifiers to monitor any newly created DownloadManagers.
  if (!notifier || notifier->GetManager() != manager) {
    notifier =
        std::make_unique<download::AllDownloadItemNotifier>(manager, this);
  }
  return manager;
}

// static
jboolean JNI_DownloadManagerService_IsSupportedMimeType(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& jmime_type) {
  std::string mime_type = ConvertJavaStringToUTF8(env, jmime_type);
  return blink::IsSupportedMimeType(mime_type);
}

// static
jint JNI_DownloadManagerService_GetAutoResumptionLimit(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz) {
  std::string value  = base::GetFieldTrialParamValueByFeature(
      chrome::android::kDownloadAutoResumptionThrottling,
      kAutoResumptionLimitParamName);
  int auto_resumption_limit;
  return base::StringToInt(value, &auto_resumption_limit)
               ? auto_resumption_limit
               : kDefaultAutoResumptionLimit;
}
