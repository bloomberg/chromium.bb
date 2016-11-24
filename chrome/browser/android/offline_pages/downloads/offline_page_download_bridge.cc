// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/downloads/offline_page_download_bridge.h"

#include <vector>

#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/android/offline_pages/downloads/offline_page_infobar_delegate.h"
#include "chrome/browser/android/offline_pages/downloads/offline_page_notification_bridge.h"
#include "chrome/browser/android/offline_pages/offline_page_mhtml_archiver.h"
#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/android/offline_pages/offline_page_utils.h"
#include "chrome/browser/android/offline_pages/recent_tab_helper.h"
#include "chrome/browser/android/offline_pages/request_coordinator_factory.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/offline_pages/background/request_coordinator.h"
#include "components/offline_pages/client_namespace_constants.h"
#include "components/offline_pages/client_policy_controller.h"
#include "components/offline_pages/downloads/download_ui_item.h"
#include "components/offline_pages/offline_page_feature.h"
#include "components/offline_pages/offline_page_model.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "jni/OfflinePageDownloadBridge_jni.h"
#include "net/base/filename_util.h"
#include "url/gurl.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace offline_pages {
namespace android {

namespace {

// TODO(dewittj): Move to Download UI Adapter.
content::WebContents* GetWebContentsFromJavaTab(
    const ScopedJavaGlobalRef<jobject>& j_tab_ref) {
  JNIEnv* env = AttachCurrentThread();
  TabAndroid* tab = TabAndroid::GetNativeTab(env, j_tab_ref);
  if (!tab)
    return nullptr;

  return tab->web_contents();
}

void SavePageIfNotNavigatedAway(const GURL& original_url,
                                const ScopedJavaGlobalRef<jobject>& j_tab_ref) {
  content::WebContents* web_contents = GetWebContentsFromJavaTab(j_tab_ref);
  if (!web_contents)
    return;

  // This ignores fragment differences in URLs, bails out only if tab has
  // navigated away and not just scrolled to a fragment.
  GURL url = web_contents->GetLastCommittedURL();
  if (!OfflinePageUtils::EqualsIgnoringFragment(url, original_url))
    return;

  offline_pages::ClientId client_id;
  client_id.name_space = offline_pages::kDownloadNamespace;
  client_id.id = base::GenerateGUID();
  int64_t request_id = OfflinePageModel::kInvalidOfflineId;

  if (offline_pages::IsBackgroundLoaderForDownloadsEnabled()) {
    // Post disabled request before passing the download task to the tab helper.
    // This will keep the request persisted in case Chrome is evicted from RAM
    // or closed by the user.
    // Note: the 'disabled' status is not persisted (stored in memory) so it
    // automatically resets if Chrome is re-started.
    offline_pages::RequestCoordinator* request_coordinator =
        offline_pages::RequestCoordinatorFactory::GetForBrowserContext(
            web_contents->GetBrowserContext());
    request_id = request_coordinator->SavePageLater(
        url, client_id, true,
        RequestCoordinator::RequestAvailability::DISABLED_FOR_OFFLINER);
  }

  // Pass request_id to the current tab's helper to attempt download right from
  // the tab. If unsuccessful, it'll enable the already-queued request for
  // background offliner. Same will happen if Chrome is terminated since
  // 'disabled' status of the request is RAM-stored info.
  offline_pages::RecentTabHelper* tab_helper =
      RecentTabHelper::FromWebContents(web_contents);
  if (!tab_helper) {
    if (request_id != OfflinePageModel::kInvalidOfflineId) {
      offline_pages::RequestCoordinator* request_coordinator =
          offline_pages::RequestCoordinatorFactory::GetForBrowserContext(
              web_contents->GetBrowserContext());
      request_coordinator->EnableForOffliner(request_id, client_id);
    }
    return;
  }
  tab_helper->ObserveAndDownloadCurrentPage(client_id, request_id);

  OfflinePageNotificationBridge notification_bridge;
  notification_bridge.ShowDownloadingToast();
}

void RequestQueueDuplicateCheckDone(
    const GURL& original_url,
    const ScopedJavaGlobalRef<jobject>& j_tab_ref,
    bool has_duplicates,
    const base::Time& latest_request_time) {
  if (has_duplicates) {
    base::TimeDelta time_since_most_recent_duplicate =
        base::Time::Now() - latest_request_time;
    // Using CUSTOM_COUNTS instead of time-oriented histogram to record
    // samples in seconds rather than milliseconds.
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "OfflinePages.DownloadRequestTimeSinceDuplicateRequested",
        time_since_most_recent_duplicate.InSeconds(),
        base::TimeDelta::FromSeconds(1).InSeconds(),
        base::TimeDelta::FromDays(7).InSeconds(), 50);

    // TODO(fgorski): Additionally we could update existing request's expiration
    // period, as it is still important. Alternative would be to actually take a
    // snapshot on the spot, but that would only work if the page is loaded
    // enough.
    // This simply toasts that the item is downloading.
    OfflinePageNotificationBridge notification_bridge;
    notification_bridge.ShowDownloadingToast();
    return;
  }

  SavePageIfNotNavigatedAway(original_url, j_tab_ref);
}

void ModelDuplicateCheckDone(const GURL& original_url,
                             const ScopedJavaGlobalRef<jobject>& j_tab_ref,
                             bool has_duplicates,
                             const base::Time& latest_saved_time) {
  content::WebContents* web_contents = GetWebContentsFromJavaTab(j_tab_ref);
  if (!web_contents)
    return;

  if (has_duplicates) {
    base::TimeDelta time_since_most_recent_duplicate =
        base::Time::Now() - latest_saved_time;
    // Using CUSTOM_COUNTS instead of time-oriented histogram to record
    // samples in seconds rather than milliseconds.
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "OfflinePages.DownloadRequestTimeSinceDuplicateSaved",
        time_since_most_recent_duplicate.InSeconds(),
        base::TimeDelta::FromSeconds(1).InSeconds(),
        base::TimeDelta::FromDays(7).InSeconds(), 50);

    OfflinePageInfoBarDelegate::Create(
        base::Bind(&SavePageIfNotNavigatedAway, original_url, j_tab_ref),
        original_url, web_contents);
    return;
  }

  OfflinePageUtils::CheckExistenceOfRequestsWithURL(
      Profile::FromBrowserContext(web_contents->GetBrowserContext())
          ->GetOriginalProfile(),
      kDownloadNamespace, original_url,
      base::Bind(&RequestQueueDuplicateCheckDone, original_url, j_tab_ref));
}

void ToJavaOfflinePageDownloadItemList(
    JNIEnv* env,
    jobject j_result_obj,
    const std::vector<const DownloadUIItem*>& items) {
  for (const auto item : items) {
    Java_OfflinePageDownloadBridge_createDownloadItemAndAddToList(
        env, j_result_obj, ConvertUTF8ToJavaString(env, item->guid),
        ConvertUTF8ToJavaString(env, item->url.spec()),
        ConvertUTF16ToJavaString(env, item->title),
        ConvertUTF8ToJavaString(env, item->target_path.value()),
        item->start_time.ToJavaTime(), item->total_bytes);
  }
}

ScopedJavaLocalRef<jobject> ToJavaOfflinePageDownloadItem(
    JNIEnv* env,
    const DownloadUIItem& item) {
  return Java_OfflinePageDownloadBridge_createDownloadItem(
      env, ConvertUTF8ToJavaString(env, item.guid),
      ConvertUTF8ToJavaString(env, item.url.spec()),
      ConvertUTF16ToJavaString(env, item.title),
      ConvertUTF8ToJavaString(env, item.target_path.value()),
      item.start_time.ToJavaTime(), item.total_bytes);
}

std::vector<int64_t> FilterRequestsByGuid(
    std::vector<std::unique_ptr<SavePageRequest>> requests,
    const std::string& guid,
    ClientPolicyController* policy_controller) {
  std::vector<int64_t> request_ids;
  for (const auto& request : requests) {
    if (request->client_id().id == guid &&
        policy_controller->IsSupportedByDownload(
            request->client_id().name_space)) {
      request_ids.push_back(request->request_id());
    }
  }
  return request_ids;
}

void CancelRequestCallback(const MultipleItemStatuses&) {
  // Results ignored here, as UI uses observer to update itself.
}

void CancelRequestsContinuation(
    content::BrowserContext* browser_context,
    const std::string& guid,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  RequestCoordinator* coordinator =
      RequestCoordinatorFactory::GetForBrowserContext(browser_context);
  if (coordinator) {
    std::vector<int64_t> request_ids = FilterRequestsByGuid(
        std::move(requests), guid, coordinator->GetPolicyController());
    coordinator->RemoveRequests(request_ids,
                                base::Bind(&CancelRequestCallback));
  } else {
    LOG(WARNING) << "CancelRequestsContinuation has no valid coordinator.";
  }
}

void PauseRequestsContinuation(
    content::BrowserContext* browser_context,
    const std::string& guid,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  RequestCoordinator* coordinator =
      RequestCoordinatorFactory::GetForBrowserContext(browser_context);
  if (coordinator)
    coordinator->PauseRequests(FilterRequestsByGuid(
        std::move(requests), guid, coordinator->GetPolicyController()));
  else
    LOG(WARNING) << "PauseRequestsContinuation has no valid coordinator.";
}

void ResumeRequestsContinuation(
    content::BrowserContext* browser_context,
    const std::string& guid,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  RequestCoordinator* coordinator =
      RequestCoordinatorFactory::GetForBrowserContext(browser_context);
  if (coordinator)
    coordinator->ResumeRequests(FilterRequestsByGuid(
        std::move(requests), guid, coordinator->GetPolicyController()));
  else
    LOG(WARNING) << "ResumeRequestsContinuation has no valid coordinator.";
}

}  // namespace

OfflinePageDownloadBridge::OfflinePageDownloadBridge(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    DownloadUIAdapter* download_ui_adapter,
    content::BrowserContext* browser_context)
    : weak_java_ref_(env, obj),
      download_ui_adapter_(download_ui_adapter),
      browser_context_(browser_context) {
  DCHECK(download_ui_adapter_);
  download_ui_adapter_->AddObserver(this);
}

OfflinePageDownloadBridge::~OfflinePageDownloadBridge() {}

// static
bool OfflinePageDownloadBridge::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void OfflinePageDownloadBridge::Destroy(JNIEnv* env,
                                        const JavaParamRef<jobject>&) {
  download_ui_adapter_->RemoveObserver(this);
  delete this;
}

void OfflinePageDownloadBridge::GetAllItems(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_result_obj) {
  DCHECK(j_result_obj);

  std::vector<const DownloadUIItem*> items =
      download_ui_adapter_->GetAllItems();
  ToJavaOfflinePageDownloadItemList(env, j_result_obj, items);
}

ScopedJavaLocalRef<jobject> OfflinePageDownloadBridge::GetItemByGuid(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_guid) {
  std::string guid = ConvertJavaStringToUTF8(env, j_guid);
  const DownloadUIItem* item = download_ui_adapter_->GetItem(guid);
  if (item == nullptr)
    return ScopedJavaLocalRef<jobject>();
  return ToJavaOfflinePageDownloadItem(env, *item);
}

void OfflinePageDownloadBridge::DeleteItemByGuid(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_guid) {
  std::string guid = ConvertJavaStringToUTF8(env, j_guid);
  download_ui_adapter_->DeleteItem(guid);
}

jlong OfflinePageDownloadBridge::GetOfflineIdByGuid(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_guid) {
  std::string guid = ConvertJavaStringToUTF8(env, j_guid);
  return download_ui_adapter_->GetOfflineIdByGuid(guid);
}

void OfflinePageDownloadBridge::StartDownload(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_tab) {
  TabAndroid* tab = TabAndroid::GetNativeTab(env, j_tab);
  if (!tab)
    return;

  content::WebContents* web_contents = tab->web_contents();
  if (!web_contents)
    return;

  GURL url = web_contents->GetLastCommittedURL();

  ScopedJavaGlobalRef<jobject> j_tab_ref(env, j_tab);

  OfflinePageUtils::CheckExistenceOfPagesWithURL(
      tab->GetProfile()->GetOriginalProfile(), kDownloadNamespace, url,
      base::Bind(&ModelDuplicateCheckDone, url, j_tab_ref));
}

void OfflinePageDownloadBridge::CancelDownload(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_guid) {
  std::string guid = ConvertJavaStringToUTF8(env, j_guid);
  RequestCoordinator* coordinator =
      RequestCoordinatorFactory::GetForBrowserContext(browser_context_);

  if (coordinator) {
    coordinator->GetAllRequests(
        base::Bind(&CancelRequestsContinuation, browser_context_, guid));
  } else {
    LOG(WARNING) << "CancelDownload has no valid coordinator.";
  }
}

void OfflinePageDownloadBridge::PauseDownload(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_guid) {
  std::string guid = ConvertJavaStringToUTF8(env, j_guid);
  RequestCoordinator* coordinator =
      RequestCoordinatorFactory::GetForBrowserContext(browser_context_);

  if (coordinator) {
    coordinator->GetAllRequests(
        base::Bind(&PauseRequestsContinuation, browser_context_, guid));
  } else {
    LOG(WARNING) << "PauseDownload has no valid coordinator.";
  }
}

void OfflinePageDownloadBridge::ResumeDownload(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_guid) {
  std::string guid = ConvertJavaStringToUTF8(env, j_guid);
  RequestCoordinator* coordinator =
      RequestCoordinatorFactory::GetForBrowserContext(browser_context_);

  if (coordinator) {
    coordinator->GetAllRequests(
        base::Bind(&ResumeRequestsContinuation, browser_context_, guid));
  } else {
    LOG(WARNING) << "ResumeDownload has no valid coordinator.";
  }
}

void OfflinePageDownloadBridge::ItemsLoaded() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_OfflinePageDownloadBridge_downloadItemsLoaded(env, obj);
}

void OfflinePageDownloadBridge::ItemAdded(const DownloadUIItem& item) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_OfflinePageDownloadBridge_downloadItemAdded(
      env, obj, ToJavaOfflinePageDownloadItem(env, item));
}

void OfflinePageDownloadBridge::ItemDeleted(const std::string& guid) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_OfflinePageDownloadBridge_downloadItemDeleted(
      env, obj, ConvertUTF8ToJavaString(env, guid));
}

void OfflinePageDownloadBridge::ItemUpdated(const DownloadUIItem& item) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_OfflinePageDownloadBridge_downloadItemUpdated(
      env, obj, ToJavaOfflinePageDownloadItem(env, item));
}

static jlong Init(JNIEnv* env,
                  const JavaParamRef<jobject>& obj,
                  const JavaParamRef<jobject>& j_profile) {
  content::BrowserContext* browser_context =
      ProfileAndroid::FromProfileAndroid(j_profile);

  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForBrowserContext(browser_context);

  DownloadUIAdapter* adapter =
      DownloadUIAdapter::FromOfflinePageModel(offline_page_model);

  return reinterpret_cast<jlong>(
      new OfflinePageDownloadBridge(env, obj, adapter, browser_context));
}

}  // namespace android
}  // namespace offline_pages
