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
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/android/window_android_helper.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/android/content_view_core.h"
#include "jni/ContextualSearchManager_jni.h"
#include "net/url_request/url_fetcher_impl.h"

using content::ContentViewCore;

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
    bool prevent_preload,
    int selection_start_adjust,
    int selection_end_adjust) {
  // Notify the Java UX of the result.
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> j_search_term =
      base::android::ConvertUTF8ToJavaString(env, search_term.c_str());
  base::android::ScopedJavaLocalRef<jstring> j_display_text =
      base::android::ConvertUTF8ToJavaString(env, display_text.c_str());
  base::android::ScopedJavaLocalRef<jstring> j_alternate_term =
      base::android::ConvertUTF8ToJavaString(env, alternate_term.c_str());
  Java_ContextualSearchManager_onSearchTermResolutionResponse(
      env, java_manager_.obj(), is_invalid, response_code, j_search_term.obj(),
      j_display_text.obj(), j_alternate_term.obj(), prevent_preload,
      selection_start_adjust, selection_end_adjust);
}

void ContextualSearchManager::OnSurroundingTextAvailable(
    const std::string& after_text) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> j_after_text =
      base::android::ConvertUTF8ToJavaString(env, after_text.c_str());
  Java_ContextualSearchManager_onSurroundingTextAvailable(
      env,
      java_manager_.obj(),
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

bool RegisterContextualSearchManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

jlong Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  ContextualSearchManager* manager = new ContextualSearchManager(env, obj);
  return reinterpret_cast<intptr_t>(manager);
}
