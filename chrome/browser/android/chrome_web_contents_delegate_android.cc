// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/chrome_web_contents_delegate_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/protected_media_identifier_permission_context.h"
#include "chrome/browser/media/protected_media_identifier_permission_context_factory.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_modal_dialogs/javascript_dialog_manager.h"
#include "chrome/browser/ui/blocked_content/popup_blocker_tab_helper.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/find_bar/find_notification_details.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/tab_helpers.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/file_chooser_params.h"
#include "jni/ChromeWebContentsDelegateAndroid_jni.h"
#include "third_party/WebKit/public/web/WebWindowFeatures.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_f.h"

#if defined(ENABLE_PLUGINS)
#include "chrome/browser/pepper_broker_infobar_delegate.h"
#endif

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;
using content::FileChooserParams;
using content::WebContents;

namespace {

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

void ChromeWebContentsDelegateAndroid::LoadingStateChanged(
    WebContents* source, bool to_different_document) {
  bool has_stopped = source == NULL || !source->IsLoading();
  WebContentsDelegateAndroid::LoadingStateChanged(
      source, to_different_document);
  LoadProgressChanged(source, has_stopped ? 1 : 0);
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
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;

  // Create the details object.
  ScopedJavaLocalRef<jobject> details_object =
      Java_ChromeWebContentsDelegateAndroid_createFindMatchRectsDetails(
          env,
          version,
          rects.size(),
          CreateJavaRectF(env, active_rect).obj());

  // Add the rects
  for (size_t i = 0; i < rects.size(); ++i) {
      Java_ChromeWebContentsDelegateAndroid_setMatchRectByIndex(
          env,
          details_object.obj(),
          i,
          CreateJavaRectF(env, rects[i]).obj());
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

void ChromeWebContentsDelegateAndroid::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback) {
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, callback, NULL);
}

bool ChromeWebContentsDelegateAndroid::CheckMediaAccessPermission(
    content::WebContents* web_contents,
    const GURL& security_origin,
    content::MediaStreamType type) {
  return MediaCaptureDevicesDispatcher::GetInstance()
      ->CheckMediaAccessPermission(web_contents, security_origin, type);
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

WebContents* ChromeWebContentsDelegateAndroid::OpenURLFromTab(
    WebContents* source,
    const content::OpenURLParams& params) {
  WindowOpenDisposition disposition = params.disposition;
  if (!source || (disposition != CURRENT_TAB &&
                  disposition != NEW_FOREGROUND_TAB &&
                  disposition != NEW_BACKGROUND_TAB &&
                  disposition != OFF_THE_RECORD &&
                  disposition != NEW_POPUP &&
                  disposition != NEW_WINDOW)) {
    // We can't handle this here.  Give the parent a chance.
    return WebContentsDelegateAndroid::OpenURLFromTab(source, params);
  }

  Profile* profile = Profile::FromBrowserContext(source->GetBrowserContext());
  chrome::NavigateParams nav_params(profile,
                                    params.url,
                                    params.transition);
  FillNavigateParamsFromOpenURLParams(&nav_params, params);
  nav_params.source_contents = source;
  nav_params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  nav_params.user_gesture = params.user_gesture;

  PopupBlockerTabHelper* popup_blocker_helper =
      PopupBlockerTabHelper::FromWebContents(source);
  DCHECK(popup_blocker_helper);

  if ((params.disposition == NEW_POPUP ||
       params.disposition == NEW_FOREGROUND_TAB ||
       params.disposition == NEW_BACKGROUND_TAB ||
       params.disposition == NEW_WINDOW) &&
      !params.user_gesture &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisablePopupBlocking)) {
    if (popup_blocker_helper->MaybeBlockPopup(nav_params,
                                              blink::WebWindowFeatures())) {
      return NULL;
    }
  }

  if (disposition == CURRENT_TAB) {
    // Only prerender for a current-tab navigation to avoid session storage
    // namespace issues.
    nav_params.target_contents = source;
    prerender::PrerenderManager* prerender_manager =
        prerender::PrerenderManagerFactory::GetForProfile(profile);
    if (prerender_manager &&
        prerender_manager->MaybeUsePrerenderedPage(params.url, &nav_params)) {
      return nav_params.target_contents;
    }
  }

  return WebContentsDelegateAndroid::OpenURLFromTab(source, params);
}

void ChromeWebContentsDelegateAndroid::AddNewContents(
    WebContents* source,
    WebContents* new_contents,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_pos,
    bool user_gesture,
    bool* was_blocked) {
  // No code for this yet.
  DCHECK_NE(disposition, SAVE_TO_DISK);
  // Can't create a new contents for the current tab - invalid case.
  DCHECK_NE(disposition, CURRENT_TAB);

  TabHelpers::AttachTabHelpers(new_contents);

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  bool handled = false;
  if (!obj.is_null()) {
    handled = Java_ChromeWebContentsDelegateAndroid_addNewContents(
        env,
        obj.obj(),
        reinterpret_cast<intptr_t>(source),
        reinterpret_cast<intptr_t>(new_contents),
        static_cast<jint>(disposition),
        NULL,
        user_gesture);
  }

  if (was_blocked)
    *was_blocked = !handled;
  if (!handled)
    delete new_contents;
}

void ChromeWebContentsDelegateAndroid::WebContentsCreated(
    content::WebContents* source_contents, int opener_render_frame_id,
    const base::string16& frame_name, const GURL& target_url,
    content::WebContents* new_contents) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  Java_ChromeWebContentsDelegateAndroid_webContentsCreated(env, obj.obj(),
      reinterpret_cast<intptr_t>(source_contents), opener_render_frame_id,
      base::android::ConvertUTF16ToJavaString(env, frame_name).Release(),
      base::android::ConvertUTF8ToJavaString(env, target_url.spec()).Release(),
      reinterpret_cast<intptr_t>(new_contents));
}

}  // namespace android
}  // namespace chrome
