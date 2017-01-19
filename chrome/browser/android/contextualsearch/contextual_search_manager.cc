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
#include "chrome/browser/android/contextualsearch/resolved_search_term.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/contextual_search/browser/contextual_search_js_api_service_impl.h"
#include "components/contextual_search/common/overlay_page_notifier_service.mojom.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "jni/ContextualSearchManager_jni.h"
#include "net/url_request/url_fetcher_impl.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/public/cpp/interface_registry.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using content::WebContents;

// This class manages the native behavior of the Contextual Search feature.
// Instances of this class are owned by the Java ContextualSearchManager.
// Most of the work is actually done in an associated delegate to this class:
// the ContextualSearchDelegate.
ContextualSearchManager::ContextualSearchManager(JNIEnv* env,
                                                 const JavaRef<jobject>& obj) {
  java_manager_.Reset(obj);
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
  Java_ContextualSearchManager_clearNativeManager(env, java_manager_);
}

void ContextualSearchManager::Destroy(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  delete this;
}

void ContextualSearchManager::StartSearchTermResolutionRequest(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_selection,
    const JavaParamRef<jstring>& j_home_country,
    const JavaParamRef<jobject>& j_base_web_contents,
    jboolean j_may_send_base_page_url) {
  WebContents* base_web_contents =
      WebContents::FromJavaWebContents(j_base_web_contents);
  DCHECK(base_web_contents);
  std::string selection(
      base::android::ConvertJavaStringToUTF8(env, j_selection));
  std::string home_country(
      base::android::ConvertJavaStringToUTF8(env, j_home_country));
  bool may_send_base_page_url = j_may_send_base_page_url;
  // Calls back to OnSearchTermResolutionResponse.
  delegate_->StartSearchTermResolutionRequest(
      selection, home_country, base_web_contents, may_send_base_page_url);
}

void ContextualSearchManager::GatherSurroundingText(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_selection,
    const JavaParamRef<jstring>& j_home_country,
    const JavaParamRef<jobject>& j_base_web_contents,
    jboolean j_may_send_base_page_url) {
  WebContents* base_web_contents =
      WebContents::FromJavaWebContents(j_base_web_contents);
  DCHECK(base_web_contents);
  std::string selection(
      base::android::ConvertJavaStringToUTF8(env, j_selection));
  std::string home_country(
      base::android::ConvertJavaStringToUTF8(env, j_home_country));
  bool may_send_base_page_url = j_may_send_base_page_url;
  delegate_->GatherAndSaveSurroundingText(
      selection, home_country, base_web_contents, may_send_base_page_url);
}

base::android::ScopedJavaLocalRef<jstring>
ContextualSearchManager::GetTargetLanguage(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj) {
  DCHECK(delegate_);
  std::string target_language(delegate_->GetTargetLanguage());
  base::android::ScopedJavaLocalRef<jstring> j_target_language =
      base::android::ConvertUTF8ToJavaString(env, target_language.c_str());
  return j_target_language;
}

base::android::ScopedJavaLocalRef<jstring>
ContextualSearchManager::GetAcceptLanguages(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj) {
  std::string accept_languages = delegate_->GetAcceptLanguages();
  base::android::ScopedJavaLocalRef<jstring> j_accept_languages =
      base::android::ConvertUTF8ToJavaString(env, accept_languages.c_str());
  return j_accept_languages;
}

void ContextualSearchManager::OnSearchTermResolutionResponse(
    const ResolvedSearchTerm& resolved_search_term) {
  // Notify the Java UX of the result.
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> j_search_term =
      base::android::ConvertUTF8ToJavaString(
          env, resolved_search_term.search_term.c_str());
  base::android::ScopedJavaLocalRef<jstring> j_display_text =
      base::android::ConvertUTF8ToJavaString(
          env, resolved_search_term.display_text.c_str());
  base::android::ScopedJavaLocalRef<jstring> j_alternate_term =
      base::android::ConvertUTF8ToJavaString(
          env, resolved_search_term.alternate_term.c_str());
  base::android::ScopedJavaLocalRef<jstring> j_mid =
      base::android::ConvertUTF8ToJavaString(env,
                                             resolved_search_term.mid.c_str());
  base::android::ScopedJavaLocalRef<jstring> j_context_language =
      base::android::ConvertUTF8ToJavaString(
          env, resolved_search_term.context_language.c_str());
  base::android::ScopedJavaLocalRef<jstring> j_thumbnail_url =
      base::android::ConvertUTF8ToJavaString(
          env,
          resolved_search_term.thumbnail_url.c_str());
  base::android::ScopedJavaLocalRef<jstring> j_caption =
      base::android::ConvertUTF8ToJavaString(
          env, resolved_search_term.caption.c_str());
  base::android::ScopedJavaLocalRef<jstring> j_quick_action_uri =
      base::android::ConvertUTF8ToJavaString(
          env, resolved_search_term.quick_action_uri.c_str());
  Java_ContextualSearchManager_onSearchTermResolutionResponse(
      env, java_manager_, resolved_search_term.is_invalid,
      resolved_search_term.response_code, j_search_term, j_display_text,
      j_alternate_term, j_mid, resolved_search_term.prevent_preload,
      resolved_search_term.selection_start_adjust,
      resolved_search_term.selection_end_adjust, j_context_language,
      j_thumbnail_url, j_caption, j_quick_action_uri,
      resolved_search_term.quick_action_category);
}

void ContextualSearchManager::OnSurroundingTextAvailable(
    const std::string& after_text) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> j_after_text =
      base::android::ConvertUTF8ToJavaString(env, after_text.c_str());
  Java_ContextualSearchManager_onSurroundingTextAvailable(env, java_manager_,
                                                          j_after_text);
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
      env, java_manager_, j_encoding, j_surrounding_text, start_offset,
      end_offset);
}

void ContextualSearchManager::EnableContextualSearchJsApiForOverlay(
    JNIEnv* env,
    jobject obj,
    const JavaParamRef<jobject>& j_overlay_web_contents) {
  WebContents* overlay_web_contents =
      WebContents::FromJavaWebContents(j_overlay_web_contents);
  DCHECK(overlay_web_contents);
  // Tell our Overlay Notifier Service that this is currently a CS page.
  content::RenderFrameHost* render_frame_host =
      overlay_web_contents->GetRenderViewHost()->GetMainFrame();
  DCHECK(render_frame_host);
  contextual_search::mojom::OverlayPageNotifierServicePtr page_notifier_service;
  render_frame_host->GetRemoteInterfaces()->GetInterface(
      &page_notifier_service);
  DCHECK(page_notifier_service);
  page_notifier_service->NotifyIsContextualSearchOverlay();

  // Also set up the backchannel to call into this class from the JS API.
  render_frame_host->GetInterfaceRegistry()->AddInterface(
      base::Bind(&contextual_search::CreateContextualSearchJsApiService, this));
}

bool RegisterContextualSearchManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

jlong Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  ContextualSearchManager* manager = new ContextualSearchManager(env, obj);
  return reinterpret_cast<intptr_t>(manager);
}

void ContextualSearchManager::SetCaption(std::string caption,
                                         bool does_answer) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> j_caption =
      base::android::ConvertUTF8ToJavaString(env, caption.c_str());
  Java_ContextualSearchManager_onSetCaption(env, java_manager_, j_caption,
                                            does_answer);
}
