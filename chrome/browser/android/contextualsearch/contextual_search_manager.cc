// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/contextualsearch/contextual_search_manager.h"

#include <set>

#include "base/android/jni_string.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/android/contextualsearch/contextual_search_delegate.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/android/window_android_helper.h"
#include "components/history/core/browser/history_service.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "components/variations/variations_associated_data.h"
#include "components/web_contents_delegate_android/web_contents_delegate_android.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/web_contents.h"
#include "jni/ContextualSearchManager_jni.h"
#include "net/url_request/url_fetcher_impl.h"

using content::ContentViewCore;

namespace {

const int kHistoryDeletionWindowSeconds = 2;

// Because we need a callback, this needs to exist.
void OnHistoryDeletionDone() {
}

}  // namespace

// This class manages the native behavior of the Contextual Search feature.
// Instances of this class are owned by the Java ContextualSearchManager.
// Most of the work is actually done in an associated delegate to this class:
// the ContextualSearchDelegate.
ContextualSearchManager::ContextualSearchManager(JNIEnv* env, jobject obj) {
  java_manager_.Reset(env, obj);
  Java_ContextualSearchManager_setNativeManager(
      env, obj, reinterpret_cast<intptr_t>(this));
  Profile* profile = ProfileManager::GetActiveUserProfile();
  delegate_.reset(new ContextualSearchDelegate(
      profile->GetRequestContext(),
      TemplateURLServiceFactory::GetForProfile(profile),
      base::Bind(&ContextualSearchManager::OnSearchTermResolutionResponse,
                 base::Unretained(this)),
      base::Bind(&ContextualSearchManager::OnSurroundingTextAvailable,
                 base::Unretained(this)),
      base::Bind(&ContextualSearchManager::OnIcingSelectionAvailable,
                 base::Unretained(this))));
}

ContextualSearchManager::~ContextualSearchManager() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextualSearchManager_clearNativeManager(env, java_manager_.obj());
}

void ContextualSearchManager::Destroy(JNIEnv* env, jobject obj) { delete this; }

void ContextualSearchManager::StartSearchTermResolutionRequest(
    JNIEnv* env,
    jobject obj,
    jstring j_selection,
    jboolean j_use_resolved_search_term,
    jobject j_base_content_view_core,
    jboolean j_may_send_base_page_url) {
  ContentViewCore* base_content_view_core =
      ContentViewCore::GetNativeContentViewCore(env, j_base_content_view_core);
  DCHECK(base_content_view_core);
  std::string selection(
      base::android::ConvertJavaStringToUTF8(env, j_selection));
  bool use_resolved_search_term = j_use_resolved_search_term;
  bool may_send_base_page_url = j_may_send_base_page_url;
  // Calls back to OnSearchTermResolutionResponse.
  delegate_->StartSearchTermResolutionRequest(
      selection, use_resolved_search_term, base_content_view_core,
      may_send_base_page_url);
}

void ContextualSearchManager::GatherSurroundingText(
    JNIEnv* env,
    jobject obj,
    jstring j_selection,
    jboolean j_use_resolved_search_term,
    jobject j_base_content_view_core,
    jboolean j_may_send_base_page_url) {
  ContentViewCore* base_content_view_core =
      ContentViewCore::GetNativeContentViewCore(env, j_base_content_view_core);
  DCHECK(base_content_view_core);
  std::string selection(
      base::android::ConvertJavaStringToUTF8(env, j_selection));
  bool use_resolved_search_term = j_use_resolved_search_term;
  bool may_send_base_page_url = j_may_send_base_page_url;
  delegate_->GatherAndSaveSurroundingText(selection, use_resolved_search_term,
                                          base_content_view_core,
                                          may_send_base_page_url);
}

void ContextualSearchManager::OnSearchTermResolutionResponse(
    bool is_invalid,
    int response_code,
    const std::string& search_term,
    const std::string& display_text,
    const std::string& alternate_term,
    bool prevent_preload) {
  // Notify the Java UX of the result.
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> j_search_term =
      base::android::ConvertUTF8ToJavaString(env, search_term.c_str());
  base::android::ScopedJavaLocalRef<jstring> j_display_text =
      base::android::ConvertUTF8ToJavaString(env, display_text.c_str());
  base::android::ScopedJavaLocalRef<jstring> j_alternate_term =
      base::android::ConvertUTF8ToJavaString(env, alternate_term.c_str());
  Java_ContextualSearchManager_onSearchTermResolutionResponse(
      env,
      java_manager_.obj(),
      is_invalid,
      response_code,
      j_search_term.obj(),
      j_display_text.obj(),
      j_alternate_term.obj(),
      prevent_preload);
}

void ContextualSearchManager::OnSurroundingTextAvailable(
    const std::string& before_text,
    const std::string& after_text) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> j_before_text =
      base::android::ConvertUTF8ToJavaString(env, before_text.c_str());
  base::android::ScopedJavaLocalRef<jstring> j_after_text =
      base::android::ConvertUTF8ToJavaString(env, after_text.c_str());
  Java_ContextualSearchManager_onSurroundingTextAvailable(
      env,
      java_manager_.obj(),
      j_before_text.obj(),
      j_after_text.obj());
}

void ContextualSearchManager::OnIcingSelectionAvailable(
    const std::string& encoding,
    const base::string16& surrounding_text,
    size_t start_offset,
    size_t end_offset) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> j_encoding =
      base::android::ConvertUTF8ToJavaString(env, encoding.c_str());
  base::android::ScopedJavaLocalRef<jstring> j_surrounding_text =
      base::android::ConvertUTF16ToJavaString(env, surrounding_text.c_str());
  Java_ContextualSearchManager_onIcingSelectionAvailable(
      env, java_manager_.obj(), j_encoding.obj(), j_surrounding_text.obj(),
      start_offset, end_offset);
}

void ContextualSearchManager::RemoveLastSearchVisit(
    JNIEnv* env,
    jobject obj,
    jstring search_url,
    jlong search_start_time_ms) {
  // The deletion window is from the time a search URL was put in history, up
  // to a short amount of time later.
  base::Time begin_time = base::Time::FromJsTime(search_start_time_ms);
  base::Time end_time = begin_time +
      base::TimeDelta::FromSeconds(kHistoryDeletionWindowSeconds);

  history::HistoryService* service = HistoryServiceFactory::GetForProfile(
      ProfileManager::GetActiveUserProfile(),
      ServiceAccessType::EXPLICIT_ACCESS);
  if (service) {
    // NOTE(mathp): We are only removing |search_url| from the local history
    // because search results that are not promoted to a Tab do not make it to
    // the web history, only local.
    std::set<GURL> restrict_set;
    restrict_set.insert(
        GURL(base::android::ConvertJavaStringToUTF8(env, search_url)));
    service->ExpireHistoryBetween(
        restrict_set,
        begin_time,
        end_time,
        base::Bind(&OnHistoryDeletionDone),
        &history_task_tracker_);
  }
}

void ContextualSearchManager::SetWebContents(JNIEnv* env,
                                             jobject obj,
                                             jobject jcontent_view_core,
                                             jobject jweb_contents_delegate) {
  content::ContentViewCore* content_view_core =
      content::ContentViewCore::GetNativeContentViewCore(env,
                                                         jcontent_view_core);
  DCHECK(content_view_core);
  DCHECK(content_view_core->GetWebContents());

  // NOTE(pedrosimonetti): Takes ownership of the WebContents associated
  // with the ContentViewCore. This is to make sure that the WebContens
  // and the Compositor are in the same process.
  // TODO(pedrosimonetti): Confirm with dtrainor@ if the comment above
  // is accurate.
  web_contents_.reset(content_view_core->GetWebContents());
  // TODO(pedrosimonetti): confirm if we need this after promoting it
  // to a real tab.
  TabAndroid::AttachTabHelpers(web_contents_.get());
  WindowAndroidHelper::FromWebContents(web_contents_.get())
      ->SetWindowAndroid(content_view_core->GetWindowAndroid());
  web_contents_delegate_.reset(
      new web_contents_delegate_android::WebContentsDelegateAndroid(
          env, jweb_contents_delegate));
  web_contents_->SetDelegate(web_contents_delegate_.get());
}

void ContextualSearchManager::DestroyWebContents(JNIEnv* env, jobject jobj) {
  DCHECK(web_contents_.get());
  web_contents_.reset();
  // |web_contents_delegate_| may already be NULL at this point.
  web_contents_delegate_.reset();
}

void ContextualSearchManager::ReleaseWebContents(JNIEnv* env, jobject jboj) {
  DCHECK(web_contents_.get());
  web_contents_delegate_.reset();
  ignore_result(web_contents_.release());
}

void ContextualSearchManager::DestroyWebContentsFromContentViewCore(
    JNIEnv* env,
    jobject jobj,
    jobject jcontent_view_core) {
  content::ContentViewCore* content_view_core =
      content::ContentViewCore::GetNativeContentViewCore(env,
                                                         jcontent_view_core);
  DCHECK(content_view_core);
  DCHECK(content_view_core->GetWebContents());

  delete content_view_core->GetWebContents();
}

void ContextualSearchManager::SetInterceptNavigationDelegate(
    JNIEnv* env,
    jobject obj,
    jobject delegate,
    jobject jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);
  navigation_interception::InterceptNavigationDelegate::Associate(
      web_contents,
      make_scoped_ptr(new navigation_interception::InterceptNavigationDelegate(
          env, delegate)));
}

bool RegisterContextualSearchManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

jlong Init(JNIEnv* env, jobject obj) {
  ContextualSearchManager* manager = new ContextualSearchManager(env, obj);
  return reinterpret_cast<intptr_t>(manager);
}
