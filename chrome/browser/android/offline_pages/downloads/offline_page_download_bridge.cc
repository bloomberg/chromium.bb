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
#include "chrome/browser/android/download/download_controller_base.h"
#include "chrome/browser/android/offline_pages/downloads/offline_page_infobar_delegate.h"
#include "chrome/browser/android/offline_pages/downloads/offline_page_notification_bridge.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/offline_pages/offline_page_mhtml_archiver.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/offline_pages/offline_page_utils.h"
#include "chrome/browser/offline_pages/recent_tab_helper.h"
#include "chrome/browser/offline_pages/request_coordinator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/client_policy_controller.h"
#include "components/offline_pages/core/downloads/download_ui_item.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "jni/OfflinePageDownloadBridge_jni.h"
#include "net/base/filename_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
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

class DownloadUIAdapterDelegate : public DownloadUIAdapter::Delegate {
 public:
  explicit DownloadUIAdapterDelegate(OfflinePageModel* model);

  // DownloadUIAdapter::Delegate
  bool IsVisibleInUI(const ClientId& client_id) override;
  bool IsTemporarilyHiddenInUI(const ClientId& client_id) override;
  void SetUIAdapter(DownloadUIAdapter* ui_adapter) override;

 private:
  // Not owned, cached service pointer.
  OfflinePageModel* model_;
};

DownloadUIAdapterDelegate::DownloadUIAdapterDelegate(OfflinePageModel* model)
    : model_(model) {}

bool DownloadUIAdapterDelegate::IsVisibleInUI(const ClientId& client_id) {
  const std::string& name_space = client_id.name_space;
  return model_->GetPolicyController()->IsSupportedByDownload(name_space) &&
         base::IsValidGUID(client_id.id);
}

bool DownloadUIAdapterDelegate::IsTemporarilyHiddenInUI(
    const ClientId& client_id) {
  return false;
}

void DownloadUIAdapterDelegate::SetUIAdapter(DownloadUIAdapter* ui_adapter) {}

// TODO(dewittj): Move to Download UI Adapter.
content::WebContents* GetWebContentsFromJavaTab(
    const ScopedJavaGlobalRef<jobject>& j_tab_ref) {
  JNIEnv* env = AttachCurrentThread();
  TabAndroid* tab = TabAndroid::GetNativeTab(env, j_tab_ref);
  if (!tab)
    return nullptr;

  return tab->web_contents();
}

void SavePageIfNotNavigatedAway(const GURL& url,
                                const GURL& original_url,
                                const ScopedJavaGlobalRef<jobject>& j_tab_ref) {
  content::WebContents* web_contents = GetWebContentsFromJavaTab(j_tab_ref);
  if (!web_contents)
    return;

  // This ignores fragment differences in URLs, bails out only if tab has
  // navigated away and not just scrolled to a fragment.
  GURL current_url = web_contents->GetLastCommittedURL();
  if (!OfflinePageUtils::EqualsIgnoringFragment(current_url, url))
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
    if (request_coordinator) {
      offline_pages::RequestCoordinator::SavePageLaterParams params;
      params.url = current_url;
      params.client_id = client_id;
      params.availability =
          RequestCoordinator::RequestAvailability::DISABLED_FOR_OFFLINER;
      params.original_url = original_url;
      request_id = request_coordinator->SavePageLater(params);
    } else {
      DVLOG(1) << "SavePageIfNotNavigatedAway has no valid coordinator.";
    }
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
      if (request_coordinator)
        request_coordinator->EnableForOffliner(request_id, client_id);
      else
        DVLOG(1) << "SavePageIfNotNavigatedAway has no valid coordinator.";
    }
    return;
  }
  tab_helper->ObserveAndDownloadCurrentPage(client_id, request_id);

  OfflinePageNotificationBridge notification_bridge;
  notification_bridge.ShowDownloadingToast();
}

void DuplicateCheckDone(const GURL& url,
                        const GURL& original_url,
                        const ScopedJavaGlobalRef<jobject>& j_tab_ref,
                        OfflinePageUtils::DuplicateCheckResult result) {
  if (result == OfflinePageUtils::DuplicateCheckResult::NOT_FOUND) {
    SavePageIfNotNavigatedAway(url, original_url, j_tab_ref);
    return;
  }

  content::WebContents* web_contents = GetWebContentsFromJavaTab(j_tab_ref);
  if (!web_contents)
    return;

  bool duplicate_request_exists =
      result == OfflinePageUtils::DuplicateCheckResult::DUPLICATE_REQUEST_FOUND;
  OfflinePageInfoBarDelegate::Create(
      base::Bind(&SavePageIfNotNavigatedAway, url, original_url, j_tab_ref),
      url, duplicate_request_exists, web_contents);
}

void ToJavaOfflinePageDownloadItemList(
    JNIEnv* env,
    jobject j_result_obj,
    const std::vector<const DownloadUIItem*>& items) {
  for (const auto* item : items) {
    Java_OfflinePageDownloadBridge_createDownloadItemAndAddToList(
        env, j_result_obj, ConvertUTF8ToJavaString(env, item->guid),
        ConvertUTF8ToJavaString(env, item->url.spec()), item->download_state,
        item->download_progress_bytes,
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
      ConvertUTF8ToJavaString(env, item.url.spec()), item.download_state,
      item.download_progress_bytes,
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

content::WebContents* GetWebContentsByFrameID(int render_process_id,
                                              int render_frame_id) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (!render_frame_host)
    return NULL;
  return content::WebContents::FromRenderFrameHost(render_frame_host);
}

content::ResourceRequestInfo::WebContentsGetter GetWebContentsGetter(
    content::WebContents* web_contents) {
  // PlzNavigate: The FrameTreeNode ID should be used to access the WebContents.
  int frame_tree_node_id = web_contents->GetMainFrame()->GetFrameTreeNodeId();
  if (frame_tree_node_id != -1) {
    return base::Bind(content::WebContents::FromFrameTreeNodeId,
                      frame_tree_node_id);
  }

  // In other cases, use the RenderProcessHost ID + RenderFrameHost ID to get
  // the WebContents.
  return base::Bind(&GetWebContentsByFrameID,
                    web_contents->GetRenderProcessHost()->GetID(),
                    web_contents->GetMainFrame()->GetRoutingID());
}

void OnAcquireFileAccessPermissionDone(
    const content::ResourceRequestInfo::WebContentsGetter& web_contents_getter,
    bool granted) {
  if (!granted)
    return;

  content::WebContents* web_contents = web_contents_getter.Run();
  if (!web_contents)
    return;

  GURL url = web_contents->GetLastCommittedURL();
  if (url.is_empty())
    return;

  content::DownloadManager* dlm = content::BrowserContext::GetDownloadManager(
      web_contents->GetBrowserContext());
  std::unique_ptr<content::DownloadUrlParameters> dl_params(
      content::DownloadUrlParameters::CreateForWebContentsMainFrame(
          web_contents, url, NO_TRAFFIC_ANNOTATION_YET));

  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  // |entry| should not be null since otherwise an empty URL is returned from
  // calling GetLastCommittedURL and we should bail out earlier.
  DCHECK(entry);
  content::Referrer referrer =
      content::Referrer::SanitizeForRequest(url, entry->GetReferrer());
  dl_params->set_referrer(referrer);

  dl_params->set_prefer_cache(true);
  dl_params->set_prompt(false);
  dlm->DownloadUrl(std::move(dl_params));
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
  if (url.is_empty())
    return;

  GURL original_url =
      offline_pages::OfflinePageUtils::GetOriginalURLFromWebContents(
          web_contents);

  // If the page is not a HTML page, route to DownloadManager.
  if (!offline_pages::OfflinePageUtils::CanDownloadAsOfflinePage(
          url, web_contents->GetContentsMimeType())) {
    content::ResourceRequestInfo::WebContentsGetter web_contents_getter =
        GetWebContentsGetter(web_contents);
    DownloadControllerBase::Get()->AcquireFileAccessPermission(
        web_contents_getter,
        base::Bind(&OnAcquireFileAccessPermissionDone, web_contents_getter));
    return;
  }

  ScopedJavaGlobalRef<jobject> j_tab_ref(env, j_tab);

  OfflinePageUtils::CheckDuplicateDownloads(
      tab->GetProfile()->GetOriginalProfile(), url,
      base::Bind(&DuplicateCheckDone, url, original_url, j_tab_ref));
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

void OfflinePageDownloadBridge::ResumePendingRequestImmediately(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  RequestCoordinator* coordinator =
      RequestCoordinatorFactory::GetForBrowserContext(browser_context_);
  if (coordinator)
    coordinator->StartImmediateProcessing(base::Bind([](bool result) {}));
  else
    LOG(WARNING) << "ResumePendingRequestImmediately has no valid coordinator.";
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
  DCHECK(offline_page_model);

  DownloadUIAdapter* adapter =
      DownloadUIAdapter::FromOfflinePageModel(offline_page_model);

  if (!adapter) {
    RequestCoordinator* request_coordinator =
        RequestCoordinatorFactory::GetForBrowserContext(browser_context);
    DCHECK(request_coordinator);
    adapter = new DownloadUIAdapter(
        offline_page_model, request_coordinator,
        base::MakeUnique<DownloadUIAdapterDelegate>(offline_page_model));
    DownloadUIAdapter::AttachToOfflinePageModel(base::WrapUnique(adapter),
                                                offline_page_model);
  }

  return reinterpret_cast<jlong>(
      new OfflinePageDownloadBridge(env, obj, adapter, browser_context));
}

}  // namespace android
}  // namespace offline_pages
