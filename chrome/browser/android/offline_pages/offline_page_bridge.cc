// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/offline_page_bridge.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/android/offline_pages/offline_page_mhtml_archiver.h"
#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/android/offline_pages/offline_page_utils.h"
#include "chrome/browser/android/offline_pages/recent_tab_helper.h"
#include "chrome/browser/android/offline_pages/request_coordinator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/background/request_queue_results.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/recent_tabs/recent_tabs_ui_adapter_delegate.h"
#include "components/offline_pages/core/request_header/offline_page_header.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "jni/OfflinePageBridge_jni.h"
#include "jni/SavePageRequest_jni.h"
#include "net/base/filename_util.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace offline_pages {
namespace android {

namespace {

const char kOfflinePageBridgeKey[] = "offline-page-bridge";

void ToJavaOfflinePageList(JNIEnv* env,
                           jobject j_result_obj,
                           const std::vector<OfflinePageItem>& offline_pages) {
  for (const OfflinePageItem& offline_page : offline_pages) {
    Java_OfflinePageBridge_createOfflinePageAndAddToList(
        env, j_result_obj,
        ConvertUTF8ToJavaString(env, offline_page.url.spec()),
        offline_page.offline_id,
        ConvertUTF8ToJavaString(env, offline_page.client_id.name_space),
        ConvertUTF8ToJavaString(env, offline_page.client_id.id),
        ConvertUTF8ToJavaString(env, offline_page.file_path.value()),
        offline_page.file_size, offline_page.creation_time.ToJavaTime(),
        offline_page.access_count, offline_page.last_access_time.ToJavaTime());
  }
}

ScopedJavaLocalRef<jobject> ToJavaOfflinePageItem(
    JNIEnv* env,
    const OfflinePageItem& offline_page) {
  return Java_OfflinePageBridge_createOfflinePageItem(
      env, ConvertUTF8ToJavaString(env, offline_page.url.spec()),
      offline_page.offline_id,
      ConvertUTF8ToJavaString(env, offline_page.client_id.name_space),
      ConvertUTF8ToJavaString(env, offline_page.client_id.id),
      ConvertUTF8ToJavaString(env, offline_page.file_path.value()),
      offline_page.file_size, offline_page.creation_time.ToJavaTime(),
      offline_page.access_count, offline_page.last_access_time.ToJavaTime());
}

void CheckPagesExistOfflineCallback(
    const ScopedJavaGlobalRef<jobject>& j_callback_obj,
    const OfflinePageModel::CheckPagesExistOfflineResult& offline_pages) {
  JNIEnv* env = base::android::AttachCurrentThread();

  std::vector<std::string> offline_pages_vector;
  for (const GURL& page : offline_pages)
    offline_pages_vector.push_back(page.spec());

  ScopedJavaLocalRef<jobjectArray> j_result_array =
      base::android::ToJavaArrayOfStrings(env, offline_pages_vector);
  DCHECK(j_result_array.obj());

  Java_CheckPagesExistOfflineCallbackInternal_onResult(env, j_callback_obj,
                                                       j_result_array);
}

void MultipleOfflinePageItemCallback(
    const ScopedJavaGlobalRef<jobject>& j_result_obj,
    const ScopedJavaGlobalRef<jobject>& j_callback_obj,
    const OfflinePageModel::MultipleOfflinePageItemResult& result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ToJavaOfflinePageList(env, j_result_obj.obj(), result);
  base::android::RunCallbackAndroid(j_callback_obj, j_result_obj);
}

void SavePageCallback(const ScopedJavaGlobalRef<jobject>& j_callback_obj,
                      const GURL& url,
                      OfflinePageModel::SavePageResult result,
                      int64_t offline_id) {
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_SavePageCallback_onSavePageDone(
      env, j_callback_obj, static_cast<int>(result),
      ConvertUTF8ToJavaString(env, url.spec()), offline_id);
}

void DeletePageCallback(const ScopedJavaGlobalRef<jobject>& j_callback_obj,
                        OfflinePageModel::DeletePageResult result) {
  base::android::RunCallbackAndroid(j_callback_obj, static_cast<int>(result));
}

void SingleOfflinePageItemCallback(
    const ScopedJavaGlobalRef<jobject>& j_callback_obj,
    const OfflinePageItem* result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_result;

  if (result)
    j_result = ToJavaOfflinePageItem(env, *result);
  base::android::RunCallbackAndroid(j_callback_obj, j_result);
}

ScopedJavaLocalRef<jobjectArray> CreateJavaSavePageRequests(
    JNIEnv* env,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  ScopedJavaLocalRef<jclass> save_page_request_clazz = base::android::GetClass(
      env, "org/chromium/chrome/browser/offlinepages/SavePageRequest");
  jobjectArray joa = env->NewObjectArray(
      requests.size(), save_page_request_clazz.obj(), nullptr);
  base::android::CheckException(env);

  for (size_t i = 0; i < requests.size(); ++i) {
    SavePageRequest request = *(requests[i]);
    ScopedJavaLocalRef<jstring> name_space =
        ConvertUTF8ToJavaString(env, request.client_id().name_space);
    ScopedJavaLocalRef<jstring> id =
        ConvertUTF8ToJavaString(env, request.client_id().id);
    ScopedJavaLocalRef<jstring> url =
        ConvertUTF8ToJavaString(env, request.url().spec());

    ScopedJavaLocalRef<jobject> j_save_page_request =
        Java_SavePageRequest_create(env, (int)request.request_state(),
                                    request.request_id(), url, name_space, id);
    env->SetObjectArrayElement(joa, i, j_save_page_request.obj());
  }

  return ScopedJavaLocalRef<jobjectArray>(env, joa);
}

void OnGetAllRequestsDone(
    const ScopedJavaGlobalRef<jobject>& j_callback_obj,
    std::vector<std::unique_ptr<SavePageRequest>> all_requests) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jobjectArray> j_result_obj =
      CreateJavaSavePageRequests(env, std::move(all_requests));
  base::android::RunCallbackAndroid(j_callback_obj, j_result_obj);
}

UpdateRequestResult ToUpdateRequestResult(ItemActionStatus status) {
  switch (status) {
    case ItemActionStatus::SUCCESS:
      return UpdateRequestResult::SUCCESS;
    case ItemActionStatus::NOT_FOUND:
      return UpdateRequestResult::REQUEST_DOES_NOT_EXIST;
    case ItemActionStatus::STORE_ERROR:
      return UpdateRequestResult::STORE_FAILURE;
    case ItemActionStatus::ALREADY_EXISTS:
    default:
      NOTREACHED();
  }
  return UpdateRequestResult::STORE_FAILURE;
}

void OnRemoveRequestsDone(const ScopedJavaGlobalRef<jobject>& j_callback_obj,
                          const MultipleItemStatuses& removed_request_results) {
  JNIEnv* env = base::android::AttachCurrentThread();

  std::vector<int> update_request_results;
  std::vector<int64_t> update_request_ids;

  for (std::pair<int64_t, ItemActionStatus> remove_result :
       removed_request_results) {
    update_request_ids.emplace_back(std::get<0>(remove_result));
    update_request_results.emplace_back(
        static_cast<int>(ToUpdateRequestResult(std::get<1>(remove_result))));
  }

  ScopedJavaLocalRef<jlongArray> j_result_ids =
      base::android::ToJavaLongArray(env, update_request_ids);
  ScopedJavaLocalRef<jintArray> j_result_codes =
      base::android::ToJavaIntArray(env, update_request_results);

  Java_RequestsRemovedCallback_onResult(env, j_callback_obj, j_result_ids,
                                        j_result_codes);
}

}  // namespace

static jboolean IsOfflineBookmarksEnabled(JNIEnv* env,
                                          const JavaParamRef<jclass>& clazz) {
  return offline_pages::IsOfflineBookmarksEnabled();
}

static jboolean IsPageSharingEnabled(JNIEnv* env,
                                     const JavaParamRef<jclass>& clazz) {
  return offline_pages::IsOfflinePagesSharingEnabled();
}

static jboolean CanSavePage(JNIEnv* env,
                            const JavaParamRef<jclass>& clazz,
                            const JavaParamRef<jstring>& j_url) {
  GURL url(ConvertJavaStringToUTF8(env, j_url));
  return OfflinePageModel::CanSaveURL(url);
}

static ScopedJavaLocalRef<jobject> GetOfflinePageBridgeForProfile(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller,
    const JavaParamRef<jobject>& j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);

  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForBrowserContext(profile);

  if (offline_page_model == nullptr)
    return ScopedJavaLocalRef<jobject>();

  OfflinePageBridge* bridge = static_cast<OfflinePageBridge*>(
      offline_page_model->GetUserData(kOfflinePageBridgeKey));
  if (!bridge) {
    bridge = new OfflinePageBridge(env, profile, offline_page_model);
    offline_page_model->SetUserData(kOfflinePageBridgeKey, bridge);
  }

  return ScopedJavaLocalRef<jobject>(bridge->java_ref());
}

// static
ScopedJavaLocalRef<jobject> OfflinePageBridge::ConvertToJavaOfflinePage(
    JNIEnv* env,
    const OfflinePageItem& offline_page) {
  return ToJavaOfflinePageItem(env, offline_page);
}

OfflinePageBridge::OfflinePageBridge(JNIEnv* env,
                                     content::BrowserContext* browser_context,
                                     OfflinePageModel* offline_page_model)
    : browser_context_(browser_context),
      offline_page_model_(offline_page_model) {
  ScopedJavaLocalRef<jobject> j_offline_page_bridge =
      Java_OfflinePageBridge_create(env, reinterpret_cast<jlong>(this));
  java_ref_.Reset(j_offline_page_bridge);

  NotifyIfDoneLoading();
  offline_page_model_->AddObserver(this);
}

OfflinePageBridge::~OfflinePageBridge() {
  JNIEnv* env = base::android::AttachCurrentThread();

  // Native shutdown causes the destruction of |this|.
  Java_OfflinePageBridge_offlinePageBridgeDestroyed(env, java_ref_);
}

void OfflinePageBridge::OfflinePageModelLoaded(OfflinePageModel* model) {
  DCHECK_EQ(offline_page_model_, model);
  NotifyIfDoneLoading();
}

void OfflinePageBridge::OfflinePageAdded(OfflinePageModel* model,
                                         const OfflinePageItem& added_page) {
  DCHECK_EQ(offline_page_model_, model);
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_OfflinePageBridge_offlinePageAdded(
      env, java_ref_, ToJavaOfflinePageItem(env, added_page));
}

void OfflinePageBridge::OfflinePageDeleted(int64_t offline_id,
                                           const ClientId& client_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_OfflinePageBridge_offlinePageDeleted(env, java_ref_, offline_id,
                                            CreateClientId(env, client_id));
}

void OfflinePageBridge::CheckPagesExistOffline(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobjectArray>& j_urls_array,
    const JavaParamRef<jobject>& j_callback_obj) {
  DCHECK(j_urls_array);
  DCHECK(j_callback_obj);

  std::vector<std::string> urls;
  base::android::AppendJavaStringArrayToStringVector(env, j_urls_array.obj(),
                                                     &urls);

  std::set<GURL> page_urls;
  for (const std::string& url : urls)
    page_urls.insert(GURL(url));

  offline_page_model_->CheckPagesExistOffline(
      page_urls, base::Bind(&CheckPagesExistOfflineCallback,
                            ScopedJavaGlobalRef<jobject>(env, j_callback_obj)));
}

void OfflinePageBridge::GetAllPages(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_result_obj,
    const JavaParamRef<jobject>& j_callback_obj) {
  DCHECK(j_result_obj);
  DCHECK(j_callback_obj);

  ScopedJavaGlobalRef<jobject> j_result_ref;
  j_result_ref.Reset(env, j_result_obj);

  ScopedJavaGlobalRef<jobject> j_callback_ref;
  j_callback_ref.Reset(env, j_callback_obj);

  offline_page_model_->GetAllPages(base::Bind(&MultipleOfflinePageItemCallback,
                                              j_result_ref, j_callback_ref));
}

void OfflinePageBridge::GetPageByOfflineId(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong offline_id,
    const JavaParamRef<jobject>& j_callback_obj) {
  ScopedJavaGlobalRef<jobject> j_callback_ref;
  j_callback_ref.Reset(env, j_callback_obj);
  offline_page_model_->GetPageByOfflineId(
      offline_id, base::Bind(&SingleOfflinePageItemCallback, j_callback_ref));
}

std::vector<ClientId> getClientIdsFromObjectArrays(
    JNIEnv* env,
    const JavaParamRef<jobjectArray>& j_namespaces_array,
    const JavaParamRef<jobjectArray>& j_ids_array) {
  std::vector<std::string> name_spaces;
  std::vector<std::string> ids;
  base::android::AppendJavaStringArrayToStringVector(
      env, j_namespaces_array.obj(), &name_spaces);
  base::android::AppendJavaStringArrayToStringVector(env, j_ids_array.obj(),
                                                     &ids);
  DCHECK_EQ(name_spaces.size(), ids.size());
  std::vector<ClientId> client_ids;

  for (size_t i = 0; i < name_spaces.size(); i++) {
    offline_pages::ClientId client_id;
    client_id.name_space = name_spaces[i];
    client_id.id = ids[i];
    client_ids.emplace_back(client_id);
  }

  return client_ids;
}

void OfflinePageBridge::DeletePagesByClientId(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobjectArray>& j_namespaces_array,
    const JavaParamRef<jobjectArray>& j_ids_array,
    const JavaParamRef<jobject>& j_callback_obj) {
  ScopedJavaGlobalRef<jobject> j_callback_ref;
  j_callback_ref.Reset(env, j_callback_obj);
  std::vector<ClientId> client_ids =
      getClientIdsFromObjectArrays(env, j_namespaces_array, j_ids_array);
  offline_page_model_->DeletePagesByClientIds(
      client_ids, base::Bind(&DeletePageCallback, j_callback_ref));
}

void OfflinePageBridge::GetPagesByClientId(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_result_obj,
    const JavaParamRef<jobjectArray>& j_namespaces_array,
    const JavaParamRef<jobjectArray>& j_ids_array,
    const JavaParamRef<jobject>& j_callback_obj) {
  ScopedJavaGlobalRef<jobject> j_result_ref;
  j_result_ref.Reset(env, j_result_obj);

  ScopedJavaGlobalRef<jobject> j_callback_ref;
  j_callback_ref.Reset(env, j_callback_obj);
  std::vector<ClientId> client_ids =
      getClientIdsFromObjectArrays(env, j_namespaces_array, j_ids_array);
  offline_page_model_->GetPagesByClientIds(
      client_ids, base::Bind(&MultipleOfflinePageItemCallback, j_result_ref,
                             j_callback_ref));
}

void OfflinePageBridge::SelectPageForOnlineUrl(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_online_url,
    int tab_id,
    const JavaParamRef<jobject>& j_callback_obj) {
  DCHECK(j_callback_obj);

  ScopedJavaGlobalRef<jobject> j_callback_ref;
  j_callback_ref.Reset(env, j_callback_obj);

  OfflinePageUtils::SelectPageForURL(
      browser_context_,
      GURL(ConvertJavaStringToUTF8(env, j_online_url)),
      OfflinePageModel::URLSearchMode::SEARCH_BY_ALL_URLS,
      tab_id,
      base::Bind(&SingleOfflinePageItemCallback, j_callback_ref));
}

void OfflinePageBridge::SavePage(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_callback_obj,
    const JavaParamRef<jobject>& j_web_contents,
    const JavaParamRef<jstring>& j_namespace,
    const JavaParamRef<jstring>& j_client_id) {
  DCHECK(j_callback_obj);
  DCHECK(j_web_contents);

  ScopedJavaGlobalRef<jobject> j_callback_ref;
  j_callback_ref.Reset(env, j_callback_obj);

  OfflinePageModel::SavePageParams save_page_params;
  std::unique_ptr<OfflinePageArchiver> archiver;

  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  if (web_contents) {
    save_page_params.url = web_contents->GetLastCommittedURL();
    archiver.reset(new OfflinePageMHTMLArchiver(web_contents));
  }

  save_page_params.client_id.name_space =
      ConvertJavaStringToUTF8(env, j_namespace);
  save_page_params.client_id.id = ConvertJavaStringToUTF8(env, j_client_id);
  save_page_params.is_background = false;

  offline_page_model_->SavePage(
      save_page_params, std::move(archiver),
      base::Bind(&SavePageCallback, j_callback_ref, save_page_params.url));
}

void OfflinePageBridge::SavePageLater(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj,
                                      const JavaParamRef<jstring>& j_url,
                                      const JavaParamRef<jstring>& j_namespace,
                                      const JavaParamRef<jstring>& j_client_id,
                                      jboolean user_requested) {
  offline_pages::ClientId client_id;
  client_id.name_space = ConvertJavaStringToUTF8(env, j_namespace);
  client_id.id = ConvertJavaStringToUTF8(env, j_client_id);

  RequestCoordinator* coordinator =
      offline_pages::RequestCoordinatorFactory::GetInstance()->
          GetForBrowserContext(browser_context_);

  RequestCoordinator::SavePageLaterParams params;
  params.url = GURL(ConvertJavaStringToUTF8(env, j_url));
  params.client_id = client_id;
  params.user_requested = static_cast<bool>(user_requested);
  params.availability =
      RequestCoordinator::RequestAvailability::ENABLED_FOR_OFFLINER;
  coordinator->SavePageLater(params);
}

ScopedJavaLocalRef<jstring> OfflinePageBridge::GetOfflinePageHeaderForReload(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  if (!web_contents)
    return ScopedJavaLocalRef<jstring>();

  const offline_pages::OfflinePageHeader* offline_header =
      offline_pages::OfflinePageUtils::GetOfflineHeaderFromWebContents(
          web_contents);
  if (!offline_header)
    return ScopedJavaLocalRef<jstring>();

  // Only replaces the reason field with "reload" value that is used to trigger
  // the network conditon check again in deciding whether to load the offline
  // page. All other fields in the offline header should still carry to the
  // reload request in order to keep the consistent behavior if we do decide to
  // load the offline page. For example, "id" field should be kept in order to
  // load the same offline version again if desired.
  offline_pages::OfflinePageHeader offline_header_for_reload = *offline_header;
  offline_header_for_reload.reason =
      offline_pages::OfflinePageHeader::Reason::RELOAD;
  return ScopedJavaLocalRef<jstring>(ConvertUTF8ToJavaString(
      env, offline_header_for_reload.GetCompleteHeaderString()));
}

jboolean OfflinePageBridge::IsShowingOfflinePreview(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  if (!web_contents)
    return false;
  return offline_pages::OfflinePageUtils::IsShowingOfflinePreview(web_contents);
}

jboolean OfflinePageBridge::IsShowingDownloadButtonInErrorPage(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  if (!web_contents)
    return false;
  return offline_pages::OfflinePageUtils::IsShowingDownloadButtonInErrorPage(
      web_contents);
}

void OfflinePageBridge::GetRequestsInQueue(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_callback_obj) {
  ScopedJavaGlobalRef<jobject> j_callback_ref(j_callback_obj);

  RequestCoordinator* coordinator =
      offline_pages::RequestCoordinatorFactory::GetInstance()
          ->GetForBrowserContext(browser_context_);

  if (!coordinator) {
    // Callback with null to signal that results are unavailable.
    const JavaParamRef<jobject> empty_result(nullptr);
    base::android::RunCallbackAndroid(j_callback_obj, empty_result);
    return;
  }

  coordinator->GetAllRequests(
      base::Bind(&OnGetAllRequestsDone, j_callback_ref));
}

void OfflinePageBridge::RemoveRequestsFromQueue(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jlongArray>& j_request_ids_array,
    const JavaParamRef<jobject>& j_callback_obj) {
  std::vector<int64_t> request_ids;
  base::android::JavaLongArrayToInt64Vector(env, j_request_ids_array,
                                            &request_ids);
  ScopedJavaGlobalRef<jobject> j_callback_ref(j_callback_obj);

  RequestCoordinator* coordinator =
      offline_pages::RequestCoordinatorFactory::GetInstance()
          ->GetForBrowserContext(browser_context_);

  if (!coordinator) {
    // Callback with null to signal that results are unavailable.
    const JavaParamRef<jobject> empty_result(nullptr);
    base::android::RunCallbackAndroid(j_callback_obj, empty_result);
    return;
  }

  coordinator->RemoveRequests(
      request_ids, base::Bind(&OnRemoveRequestsDone, j_callback_ref));
}

void OfflinePageBridge::RegisterRecentTab(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj,
                                          int tab_id) {
  RequestCoordinator* request_coordinator =
      RequestCoordinatorFactory::GetForBrowserContext(browser_context_);

  RecentTabsUIAdapterDelegate* ui_adapter_delegate =
      RecentTabsUIAdapterDelegate::FromDownloadUIAdapter(
          RecentTabsUIAdapterDelegate::GetOrCreateRecentTabsUIAdapter(
              offline_page_model_, request_coordinator));
  ui_adapter_delegate->RegisterTab(tab_id);
}

void OfflinePageBridge::WillCloseTab(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_web_contents) {
  DCHECK(j_web_contents);
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  DCHECK(web_contents);
  if (!web_contents)
    return;

  RecentTabHelper* tab_helper = RecentTabHelper::FromWebContents(web_contents);
  if (tab_helper)
    tab_helper->WillCloseTab();
}

void OfflinePageBridge::UnregisterRecentTab(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj,
                                            int tab_id) {
  RequestCoordinator* request_coordinator =
      RequestCoordinatorFactory::GetForBrowserContext(browser_context_);

  RecentTabsUIAdapterDelegate* ui_adapter_delegate =
      RecentTabsUIAdapterDelegate::FromDownloadUIAdapter(
          RecentTabsUIAdapterDelegate::GetOrCreateRecentTabsUIAdapter(
              offline_page_model_, request_coordinator));
  ui_adapter_delegate->UnregisterTab(tab_id);
}

void OfflinePageBridge::NotifyIfDoneLoading() const {
  if (!offline_page_model_->is_loaded())
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_OfflinePageBridge_offlinePageModelLoaded(env, java_ref_);
}


ScopedJavaLocalRef<jobject> OfflinePageBridge::CreateClientId(
    JNIEnv* env,
    const ClientId& client_id) const {
  return Java_OfflinePageBridge_createClientId(
      env, ConvertUTF8ToJavaString(env, client_id.name_space),
      ConvertUTF8ToJavaString(env, client_id.id));
}

bool RegisterOfflinePageBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace offline_pages
