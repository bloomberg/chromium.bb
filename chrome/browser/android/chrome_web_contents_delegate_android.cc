// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/chrome_web_contents_delegate_android.h"

#include "base/android/jni_android.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_modal_dialogs/javascript_dialog_manager.h"
#include "chrome/browser/ui/find_bar/find_match_rects_details.h"
#include "chrome/browser/ui/find_bar/find_notification_details.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/media_stream_infobar_delegate.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/android/download_controller_android.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/file_chooser_params.h"
#include "content/public/common/page_transition_types.h"
#include "jni/ChromeWebContentsDelegateAndroid_jni.h"
#include "net/http/http_request_headers.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_f.h"

#if defined(ENABLE_PLUGINS)
#include "chrome/browser/pepper_broker_infobar_delegate.h"
#endif

using base::android::ScopedJavaLocalRef;
using content::FileChooserParams;
using content::NavigationController;
using content::NavigationEntry;
using content::WebContents;

namespace {

NavigationEntry* GetActiveEntry(WebContents* web_contents) {
  return web_contents->GetController().GetActiveEntry();
}

bool IsActiveNavigationGoogleSearch(WebContents* web_contents) {
  NavigationEntry* entry = GetActiveEntry(web_contents);
  content::PageTransition transition = entry->GetTransitionType();
  if (!(transition & content::PAGE_TRANSITION_GENERATED) ||
      !(transition & content::PAGE_TRANSITION_FROM_ADDRESS_BAR)) {
    return false;
  }
  GURL search_url = GoogleURLTracker::GoogleURL(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  return StartsWithASCII(entry->GetURL().spec(), search_url.spec(), false);
}

ScopedJavaLocalRef<jobject> CreateJavaRectF(
    JNIEnv* env,
    const gfx::RectF& rect) {
  return ScopedJavaLocalRef<jobject>(
      Java_ChromeWebContentsDelegateAndroid_createRectF(env,
                                                        rect.x(),
                                                        rect.y(),
                                                        rect.right(),
                                                        rect.bottom()));
}

ScopedJavaLocalRef<jobject> CreateJavaRect(
    JNIEnv* env,
    const gfx::Rect& rect) {
  return ScopedJavaLocalRef<jobject>(
      Java_ChromeWebContentsDelegateAndroid_createRect(
          env,
          static_cast<int>(rect.x()),
          static_cast<int>(rect.y()),
          static_cast<int>(rect.right()),
          static_cast<int>(rect.bottom())));
}

}  // anonymous namespace

namespace chrome {
namespace android {

ChromeWebContentsDelegateAndroid::ChromeWebContentsDelegateAndroid(JNIEnv* env,
                                                                   jobject obj)
    : WebContentsDelegateAndroid(env, obj) {
}

ChromeWebContentsDelegateAndroid::~ChromeWebContentsDelegateAndroid() {
  notification_registrar_.RemoveAll();
}

// Register native methods.
bool RegisterChromeWebContentsDelegateAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void ChromeWebContentsDelegateAndroid::RunFileChooser(
    WebContents* web_contents,
    const FileChooserParams& params) {
  FileSelectHelper::RunFileChooser(web_contents, params);
}

void ChromeWebContentsDelegateAndroid::CloseContents(
    WebContents* web_contents) {
  // Prevent dangling registrations assigned to closed web contents.
  if (notification_registrar_.IsRegistered(this,
      chrome::NOTIFICATION_FIND_RESULT_AVAILABLE,
      content::Source<WebContents>(web_contents))) {
    notification_registrar_.Remove(this,
        chrome::NOTIFICATION_FIND_RESULT_AVAILABLE,
        content::Source<WebContents>(web_contents));
  }

  WebContentsDelegateAndroid::CloseContents(web_contents);
}

void ChromeWebContentsDelegateAndroid::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_FIND_RESULT_AVAILABLE:
      OnFindResultAvailable(
          content::Source<WebContents>(source).ptr(),
          content::Details<FindNotificationDetails>(details).ptr());
      break;
    default:
      NOTREACHED() << "Unexpected notification: " << type;
      break;
  }
}

void ChromeWebContentsDelegateAndroid::FindReply(
    WebContents* web_contents,
    int request_id,
    int number_of_matches,
    const gfx::Rect& selection_rect,
    int active_match_ordinal,
    bool final_update) {
  if (!notification_registrar_.IsRegistered(this,
      chrome::NOTIFICATION_FIND_RESULT_AVAILABLE,
      content::Source<WebContents>(web_contents))) {
    notification_registrar_.Add(this,
        chrome::NOTIFICATION_FIND_RESULT_AVAILABLE,
        content::Source<WebContents>(web_contents));
  }

  FindTabHelper* find_tab_helper = FindTabHelper::FromWebContents(web_contents);
  find_tab_helper->HandleFindReply(request_id,
                                   number_of_matches,
                                   selection_rect,
                                   active_match_ordinal,
                                   final_update);
}

void ChromeWebContentsDelegateAndroid::OnFindResultAvailable(
    WebContents* web_contents,
    const FindNotificationDetails* find_result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;

  ScopedJavaLocalRef<jobject> selection_rect = CreateJavaRect(
      env, find_result->selection_rect());

  // Create the details object.
  ScopedJavaLocalRef<jobject> details_object =
      Java_ChromeWebContentsDelegateAndroid_createFindNotificationDetails(
          env,
          find_result->number_of_matches(),
          selection_rect.obj(),
          find_result->active_match_ordinal(),
          find_result->final_update());

  Java_ChromeWebContentsDelegateAndroid_onFindResultAvailable(
      env,
      obj.obj(),
      details_object.obj());
}

void ChromeWebContentsDelegateAndroid::FindMatchRectsReply(
    WebContents* web_contents,
    int version,
    const std::vector<gfx::RectF>& rects,
    const gfx::RectF& active_rect) {
  FindMatchRectsDetails match_rects(version, rects, active_rect);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_FIND_MATCH_RECTS_AVAILABLE,
      content::Source<WebContents>(web_contents),
      content::Details<FindMatchRectsDetails>(&match_rects));

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;

  // Create the details object.
  ScopedJavaLocalRef<jobject> details_object =
      Java_ChromeWebContentsDelegateAndroid_createFindMatchRectsDetails(
          env,
          match_rects.version(),
          match_rects.rects().size(),
          CreateJavaRectF(env, match_rects.active_rect()).obj());

  // Add the rects
  for (size_t i = 0; i < match_rects.rects().size(); ++i) {
      Java_ChromeWebContentsDelegateAndroid_setMatchRectByIndex(
          env,
          details_object.obj(),
          i,
          CreateJavaRectF(env, match_rects.rects()[i]).obj());
  }

  Java_ChromeWebContentsDelegateAndroid_onFindMatchRectsAvailable(
      env,
      obj.obj(),
      details_object.obj());
}

content::JavaScriptDialogManager*
ChromeWebContentsDelegateAndroid::GetJavaScriptDialogManager() {
  return GetJavaScriptDialogManagerInstance();
}

void ChromeWebContentsDelegateAndroid::CanDownload(
    content::RenderViewHost* source,
    int request_id,
    const std::string& request_method,
    const base::Callback<void(bool)>& callback) {
  if (request_method == net::HttpRequestHeaders::kGetMethod) {
    content::DownloadControllerAndroid::Get()->CreateGETDownload(
        source, request_id);
    callback.Run(false);
  }
  // DownloadControllerAndroid::OnPostDownloadStarted() is called for the
  // started download by the default DownloadUIController::Delegate
  // implementation.
  callback.Run(true);
}

void ChromeWebContentsDelegateAndroid::DidNavigateToPendingEntry(
    content::WebContents* source) {
  navigation_start_time_ = base::TimeTicks::Now();
}

void ChromeWebContentsDelegateAndroid::DidNavigateMainFramePostCommit(
    content::WebContents* source) {
  if (!IsActiveNavigationGoogleSearch(source))
    return;

  base::TimeDelta time_delta = base::TimeTicks::Now() - navigation_start_time_;
  if (GetActiveEntry(source)->GetURL().SchemeIsSecure()) {
    UMA_HISTOGRAM_TIMES("Omnibox.GoogleSearch.SecureSearchTime", time_delta);
  } else {
    UMA_HISTOGRAM_TIMES("Omnibox.GoogleSearch.SearchTime", time_delta);
  }
}

void ChromeWebContentsDelegateAndroid::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback) {
  MediaStreamInfoBarDelegate::Create(web_contents, request, callback);
}

bool ChromeWebContentsDelegateAndroid::RequestPpapiBrokerPermission(
    WebContents* web_contents,
    const GURL& url,
    const base::FilePath& plugin_path,
    const base::Callback<void(bool)>& callback) {
#if defined(ENABLE_PLUGINS)
    PepperBrokerInfoBarDelegate::Create(
        web_contents, url, plugin_path, callback);
    return true;
#else
    return false;
#endif
}


}  // namespace android
}  // namespace chrome
