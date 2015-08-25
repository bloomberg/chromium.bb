// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/bottombar/contextualsearch/contextual_search_panel.h"

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
#include "jni/ContextualSearchPanel_jni.h"
#include "net/url_request/url_fetcher_impl.h"

using content::ContentViewCore;

namespace {

const int kHistoryDeletionWindowSeconds = 2;

// Because we need a callback, this needs to exist.
void OnHistoryDeletionDone() {
}

}  // namespace

// This class manages the native behavior of the Contextual Search feature.
// Instances of this class are owned by the Java ContextualSearchPanel.
ContextualSearchPanel::ContextualSearchPanel(JNIEnv* env, jobject obj) {
  java_manager_.Reset(env, obj);
  Java_ContextualSearchPanel_setNativePanelContentPtr(
      env, obj, reinterpret_cast<intptr_t>(this));
}

ContextualSearchPanel::~ContextualSearchPanel() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextualSearchPanel_clearNativePanelContentPtr(
      env, java_manager_.obj());
}

void ContextualSearchPanel::Destroy(JNIEnv* env, jobject obj) { delete this; }

void ContextualSearchPanel::RemoveLastHistoryEntry(
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

void ContextualSearchPanel::SetWebContents(JNIEnv* env,
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

void ContextualSearchPanel::DestroyWebContents(JNIEnv* env, jobject jobj) {
  DCHECK(web_contents_.get());
  web_contents_.reset();
  // |web_contents_delegate_| may already be NULL at this point.
  web_contents_delegate_.reset();
}

void ContextualSearchPanel::ReleaseWebContents(JNIEnv* env, jobject jboj) {
  DCHECK(web_contents_.get());
  web_contents_delegate_.reset();
  ignore_result(web_contents_.release());
}

void ContextualSearchPanel::SetInterceptNavigationDelegate(
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

bool RegisterContextualSearchPanel(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

jlong Init(JNIEnv* env, jobject obj) {
  ContextualSearchPanel* manager = new ContextualSearchPanel(env, obj);
  return reinterpret_cast<intptr_t>(manager);
}
