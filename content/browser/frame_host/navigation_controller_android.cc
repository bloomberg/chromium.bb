// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_controller_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "jni/NavigationControllerImpl_jni.h"
#include "ui/gfx/android/java_bitmap.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
namespace {

// static
static base::android::ScopedJavaLocalRef<jobject> CreateJavaNavigationEntry(
    JNIEnv* env,
    content::NavigationEntry* entry,
    int index) {
  DCHECK(entry);

  // Get the details of the current entry
  ScopedJavaLocalRef<jstring> j_url(
      ConvertUTF8ToJavaString(env, entry->GetURL().spec()));
  ScopedJavaLocalRef<jstring> j_virtual_url(
      ConvertUTF8ToJavaString(env, entry->GetVirtualURL().spec()));
  ScopedJavaLocalRef<jstring> j_original_url(
      ConvertUTF8ToJavaString(env, entry->GetOriginalRequestURL().spec()));
  ScopedJavaLocalRef<jstring> j_title(
      ConvertUTF16ToJavaString(env, entry->GetTitle()));
  ScopedJavaLocalRef<jobject> j_bitmap;
  const content::FaviconStatus& status = entry->GetFavicon();
  if (status.valid && status.image.ToSkBitmap()->getSize() > 0)
    j_bitmap = gfx::ConvertToJavaBitmap(status.image.ToSkBitmap());

  return content::Java_NavigationControllerImpl_createNavigationEntry(
      env,
      index,
      j_url.obj(),
      j_virtual_url.obj(),
      j_original_url.obj(),
      j_title.obj(),
      j_bitmap.obj());
}

static void AddNavigationEntryToHistory(JNIEnv* env,
                                        jobject history,
                                        content::NavigationEntry* entry,
                                        int index) {
  content::Java_NavigationControllerImpl_addToNavigationHistory(
      env,
      history,
      CreateJavaNavigationEntry(env, entry, index).obj());
}

}  // namespace

namespace content {

// static
bool NavigationControllerAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

NavigationControllerAndroid::NavigationControllerAndroid(
    NavigationController* navigation_controller)
    : navigation_controller_(navigation_controller) {
  JNIEnv* env = AttachCurrentThread();
  obj_.Reset(env,
             Java_NavigationControllerImpl_create(
                 env, reinterpret_cast<intptr_t>(this)).obj());
}

NavigationControllerAndroid::~NavigationControllerAndroid() {
  Java_NavigationControllerImpl_destroy(AttachCurrentThread(), obj_.obj());
}

base::android::ScopedJavaLocalRef<jobject>
NavigationControllerAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(obj_);
}

jboolean NavigationControllerAndroid::CanGoBack(JNIEnv* env, jobject obj) {
  return navigation_controller_->CanGoBack();
}

jboolean NavigationControllerAndroid::CanGoForward(JNIEnv* env,
                                                   jobject obj) {
  return navigation_controller_->CanGoForward();
}

jboolean NavigationControllerAndroid::CanGoToOffset(JNIEnv* env,
                                                    jobject obj,
                                                    jint offset) {
  return navigation_controller_->CanGoToOffset(offset);
}

void NavigationControllerAndroid::GoBack(JNIEnv* env, jobject obj) {
  navigation_controller_->GoBack();
}

void NavigationControllerAndroid::GoForward(JNIEnv* env, jobject obj) {
  navigation_controller_->GoForward();
}

void NavigationControllerAndroid::GoToOffset(JNIEnv* env,
                                             jobject obj,
                                             jint offset) {
  navigation_controller_->GoToOffset(offset);
}

void NavigationControllerAndroid::LoadIfNecessary(JNIEnv* env, jobject obj) {
  navigation_controller_->LoadIfNecessary();
}

void NavigationControllerAndroid::ContinuePendingReload(JNIEnv* env,
                                                        jobject obj) {
  navigation_controller_->ContinuePendingReload();
}

void NavigationControllerAndroid::Reload(JNIEnv* env,
                                         jobject obj,
                                         jboolean check_for_repost) {
  navigation_controller_->Reload(check_for_repost);
}

void NavigationControllerAndroid::ReloadIgnoringCache(
    JNIEnv* env,
    jobject obj,
    jboolean check_for_repost) {
  navigation_controller_->ReloadIgnoringCache(check_for_repost);
}

void NavigationControllerAndroid::RequestRestoreLoad(JNIEnv* env, jobject obj) {
  navigation_controller_->SetNeedsReload();
}

void NavigationControllerAndroid::CancelPendingReload(JNIEnv* env,
                                                      jobject obj) {
  navigation_controller_->CancelPendingReload();
}

void NavigationControllerAndroid::GoToNavigationIndex(JNIEnv* env,
                                                      jobject obj,
                                                      jint index) {
  navigation_controller_->GoToIndex(index);
}

void NavigationControllerAndroid::LoadUrl(JNIEnv* env,
                                          jobject obj,
                                          jstring url,
                                          jint load_url_type,
                                          jint transition_type,
                                          jstring j_referrer_url,
                                          jint referrer_policy,
                                          jint ua_override_option,
                                          jstring extra_headers,
                                          jbyteArray post_data,
                                          jstring base_url_for_data_url,
                                          jstring virtual_url_for_data_url,
                                          jboolean can_load_local_resources,
                                          jboolean is_renderer_initiated) {
  DCHECK(url);
  NavigationController::LoadURLParams params(
      GURL(ConvertJavaStringToUTF8(env, url)));

  params.load_type =
      static_cast<NavigationController::LoadURLType>(load_url_type);
  params.transition_type = ui::PageTransitionFromInt(transition_type);
  params.override_user_agent =
      static_cast<NavigationController::UserAgentOverrideOption>(
          ua_override_option);
  params.can_load_local_resources = can_load_local_resources;
  params.is_renderer_initiated = is_renderer_initiated;

  if (extra_headers)
    params.extra_headers = ConvertJavaStringToUTF8(env, extra_headers);

  if (post_data) {
    std::vector<uint8> http_body_vector;
    base::android::JavaByteArrayToByteVector(env, post_data, &http_body_vector);
    params.browser_initiated_post_data =
        base::RefCountedBytes::TakeVector(&http_body_vector);
  }

  if (base_url_for_data_url) {
    params.base_url_for_data_url =
        GURL(ConvertJavaStringToUTF8(env, base_url_for_data_url));
  }

  if (virtual_url_for_data_url) {
    params.virtual_url_for_data_url =
        GURL(ConvertJavaStringToUTF8(env, virtual_url_for_data_url));
  }

  if (j_referrer_url) {
    params.referrer = content::Referrer(
        GURL(ConvertJavaStringToUTF8(env, j_referrer_url)),
        static_cast<blink::WebReferrerPolicy>(referrer_policy));
  }

  navigation_controller_->LoadURLWithParams(params);
}

void NavigationControllerAndroid::ClearHistory(JNIEnv* env, jobject obj) {
  // TODO(creis): Do callers of this need to know if it fails?
  if (navigation_controller_->CanPruneAllButLastCommitted())
    navigation_controller_->PruneAllButLastCommitted();
}

jint NavigationControllerAndroid::GetNavigationHistory(JNIEnv* env,
                                                       jobject obj,
                                                       jobject history) {
  // Iterate through navigation entries to populate the list
  int count = navigation_controller_->GetEntryCount();
  for (int i = 0; i < count; ++i) {
    AddNavigationEntryToHistory(
        env, history, navigation_controller_->GetEntryAtIndex(i), i);
  }

  return navigation_controller_->GetCurrentEntryIndex();
}

void NavigationControllerAndroid::GetDirectedNavigationHistory(
    JNIEnv* env,
    jobject obj,
    jobject history,
    jboolean is_forward,
    jint max_entries) {
  // Iterate through navigation entries to populate the list
  int count = navigation_controller_->GetEntryCount();
  int num_added = 0;
  int increment_value = is_forward ? 1 : -1;
  for (int i = navigation_controller_->GetCurrentEntryIndex() + increment_value;
       i >= 0 && i < count;
       i += increment_value) {
    if (num_added >= max_entries)
      break;

    AddNavigationEntryToHistory(
        env, history, navigation_controller_->GetEntryAtIndex(i), i);
    num_added++;
  }
}

ScopedJavaLocalRef<jstring>
NavigationControllerAndroid::GetOriginalUrlForVisibleNavigationEntry(
    JNIEnv* env,
    jobject obj) {
  NavigationEntry* entry = navigation_controller_->GetVisibleEntry();
  if (entry == NULL)
    return ScopedJavaLocalRef<jstring>(env, NULL);
  return ConvertUTF8ToJavaString(env, entry->GetOriginalRequestURL().spec());
}

void NavigationControllerAndroid::ClearSslPreferences(JNIEnv* env,
                                                      jobject obj) {
  content::SSLHostStateDelegate* delegate =
      navigation_controller_->GetBrowserContext()->GetSSLHostStateDelegate();
  if (delegate)
    delegate->Clear();
}

bool NavigationControllerAndroid::GetUseDesktopUserAgent(JNIEnv* env,
                                                         jobject obj) {
  NavigationEntry* entry = navigation_controller_->GetVisibleEntry();
  return entry && entry->GetIsOverridingUserAgent();
}

void NavigationControllerAndroid::SetUseDesktopUserAgent(
    JNIEnv* env,
    jobject obj,
    jboolean enabled,
    jboolean reload_on_state_change) {
  if (GetUseDesktopUserAgent(env, obj) == enabled)
    return;

  // Make sure the navigation entry actually exists.
  NavigationEntry* entry = navigation_controller_->GetVisibleEntry();
  if (!entry)
    return;

  // Set the flag in the NavigationEntry.
  entry->SetIsOverridingUserAgent(enabled);

  // Send the override to the renderer.
  if (reload_on_state_change) {
    // Reloading the page will send the override down as part of the
    // navigation IPC message.
    navigation_controller_->ReloadOriginalRequestURL(false);
  }
}

base::android::ScopedJavaLocalRef<jobject>
NavigationControllerAndroid::GetPendingEntry(JNIEnv* env, jobject obj) {
  content::NavigationEntry* entry = navigation_controller_->GetPendingEntry();

  if (!entry)
    return base::android::ScopedJavaLocalRef<jobject>();

  return CreateJavaNavigationEntry(
      env, entry, navigation_controller_->GetPendingEntryIndex());
}

}  // namespace content
