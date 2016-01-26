// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_web_contents_delegate_android.h"

#include <stddef.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "chrome/browser/android/feature_utilities.h"
#include "chrome/browser/android/hung_renderer_infobar_delegate.h"
#include "chrome/browser/android/media/media_throttle_infobar_delegate.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/media_stream_capture_indicator.h"
#include "chrome/browser/media/protected_media_identifier_permission_context.h"
#include "chrome/browser/media/protected_media_identifier_permission_context_factory.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/bluetooth_chooser_android.h"
#include "chrome/browser/ui/blocked_content/popup_blocker_tab_helper.h"
#include "chrome/browser/ui/find_bar/find_notification_details.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/tab_helpers.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "components/app_modal/javascript_dialog_manager.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/file_chooser_params.h"
#include "jni/TabWebContentsDelegateAndroid_jni.h"
#include "third_party/WebKit/public/web/WebWindowFeatures.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

#if defined(ENABLE_PLUGINS)
#include "chrome/browser/pepper_broker_infobar_delegate.h"
#endif

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;
using content::BluetoothChooser;
using content::FileChooserParams;
using content::WebContents;

namespace {

ScopedJavaLocalRef<jobject> CreateJavaRectF(
    JNIEnv* env,
    const gfx::RectF& rect) {
  return ScopedJavaLocalRef<jobject>(
      Java_TabWebContentsDelegateAndroid_createRectF(env,
                                                        rect.x(),
                                                        rect.y(),
                                                        rect.right(),
                                                        rect.bottom()));
}

ScopedJavaLocalRef<jobject> CreateJavaRect(
    JNIEnv* env,
    const gfx::Rect& rect) {
  return ScopedJavaLocalRef<jobject>(
      Java_TabWebContentsDelegateAndroid_createRect(
          env,
          static_cast<int>(rect.x()),
          static_cast<int>(rect.y()),
          static_cast<int>(rect.right()),
          static_cast<int>(rect.bottom())));
}

infobars::InfoBar* FindHungRendererInfoBar(InfoBarService* infobar_service) {
  DCHECK(infobar_service);
  for (size_t i = 0; i < infobar_service->infobar_count(); ++i) {
    infobars::InfoBar* infobar = infobar_service->infobar_at(i);
    if (infobar->delegate()->AsHungRendererInfoBarDelegate())
      return infobar;
  }
  return nullptr;
}

}  // anonymous namespace

namespace chrome {
namespace android {

TabWebContentsDelegateAndroid::TabWebContentsDelegateAndroid(JNIEnv* env,
                                                                   jobject obj)
    : WebContentsDelegateAndroid(env, obj) {
}

TabWebContentsDelegateAndroid::~TabWebContentsDelegateAndroid() {
  notification_registrar_.RemoveAll();
}

// Register native methods.
bool RegisterTabWebContentsDelegateAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void TabWebContentsDelegateAndroid::LoadingStateChanged(
    WebContents* source, bool to_different_document) {
  bool has_stopped = source == NULL || !source->IsLoading();
  WebContentsDelegateAndroid::LoadingStateChanged(
      source, to_different_document);
  LoadProgressChanged(source, has_stopped ? 1 : 0);
}

void TabWebContentsDelegateAndroid::RunFileChooser(
    WebContents* web_contents,
    const FileChooserParams& params) {
  FileSelectHelper::RunFileChooser(web_contents, params);
}

scoped_ptr<BluetoothChooser> TabWebContentsDelegateAndroid::RunBluetoothChooser(
    content::WebContents* web_contents,
    const BluetoothChooser::EventHandler& event_handler,
    const url::Origin& origin) {
  return make_scoped_ptr(
      new BluetoothChooserAndroid(web_contents, event_handler, origin));
}

void TabWebContentsDelegateAndroid::CloseContents(
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

bool TabWebContentsDelegateAndroid::ShouldFocusLocationBarByDefault(
    WebContents* source) {
  const content::NavigationEntry* entry =
      source->GetController().GetActiveEntry();
  if (entry) {
    GURL url = entry->GetURL();
    GURL virtual_url = entry->GetVirtualURL();
    if ((url.SchemeIs(chrome::kChromeUINativeScheme) &&
        url.host_piece() == chrome::kChromeUINewTabHost) ||
        (virtual_url.SchemeIs(chrome::kChromeUINativeScheme) &&
        virtual_url.host_piece() == chrome::kChromeUINewTabHost)) {
      return true;
    }
  }
  return false;
}


void TabWebContentsDelegateAndroid::Observe(
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

blink::WebDisplayMode TabWebContentsDelegateAndroid::GetDisplayMode(
    const WebContents* web_contents) const {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return blink::WebDisplayModeUndefined;

  return static_cast<blink::WebDisplayMode>(
      Java_TabWebContentsDelegateAndroid_getDisplayMode(
          env, obj.obj()));
}

void TabWebContentsDelegateAndroid::FindReply(
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

void TabWebContentsDelegateAndroid::OnFindResultAvailable(
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
      Java_TabWebContentsDelegateAndroid_createFindNotificationDetails(
          env,
          find_result->number_of_matches(),
          selection_rect.obj(),
          find_result->active_match_ordinal(),
          find_result->final_update());

  Java_TabWebContentsDelegateAndroid_onFindResultAvailable(
      env,
      obj.obj(),
      details_object.obj());
}

void TabWebContentsDelegateAndroid::FindMatchRectsReply(
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
      Java_TabWebContentsDelegateAndroid_createFindMatchRectsDetails(
          env,
          version,
          rects.size(),
          CreateJavaRectF(env, active_rect).obj());

  // Add the rects
  for (size_t i = 0; i < rects.size(); ++i) {
      Java_TabWebContentsDelegateAndroid_setMatchRectByIndex(
          env,
          details_object.obj(),
          i,
          CreateJavaRectF(env, rects[i]).obj());
  }

  Java_TabWebContentsDelegateAndroid_onFindMatchRectsAvailable(
      env,
      obj.obj(),
      details_object.obj());
}

content::JavaScriptDialogManager*
TabWebContentsDelegateAndroid::GetJavaScriptDialogManager(
    WebContents* source) {
  return app_modal::JavaScriptDialogManager::GetInstance();
}

void TabWebContentsDelegateAndroid::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback) {
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, callback, NULL);
}

bool TabWebContentsDelegateAndroid::CheckMediaAccessPermission(
    content::WebContents* web_contents,
    const GURL& security_origin,
    content::MediaStreamType type) {
  return MediaCaptureDevicesDispatcher::GetInstance()
      ->CheckMediaAccessPermission(web_contents, security_origin, type);
}

void TabWebContentsDelegateAndroid::RequestMediaDecodePermission(
    content::WebContents* web_contents,
    const base::Callback<void(bool)>& callback) {
  MediaThrottleInfoBarDelegate::Create(web_contents, callback);
}

bool TabWebContentsDelegateAndroid::RequestPpapiBrokerPermission(
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

WebContents* TabWebContentsDelegateAndroid::OpenURLFromTab(
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
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
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

bool TabWebContentsDelegateAndroid::ShouldResumeRequestsForCreatedWindow() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return true;

  return Java_TabWebContentsDelegateAndroid_shouldResumeRequestsForCreatedWindow(
      env,
      obj.obj());
}

void TabWebContentsDelegateAndroid::AddNewContents(
    WebContents* source,
    WebContents* new_contents,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_rect,
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
    ScopedJavaLocalRef<jobject> jsource;
    if (source)
      jsource = source->GetJavaWebContents();
    ScopedJavaLocalRef<jobject> jnew_contents;
    if (new_contents)
      jnew_contents = new_contents->GetJavaWebContents();

    handled = Java_TabWebContentsDelegateAndroid_addNewContents(
        env,
        obj.obj(),
        jsource.obj(),
        jnew_contents.obj(),
        static_cast<jint>(disposition),
        NULL,
        user_gesture);
  }

  if (was_blocked)
    *was_blocked = !handled;
  if (!handled)
    delete new_contents;
}

bool TabWebContentsDelegateAndroid::RequestAppBanner(
    content::WebContents* web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return false;
  return Java_TabWebContentsDelegateAndroid_requestAppBanner(env, obj.obj());
}

}  // namespace android
}  // namespace chrome

void OnRendererUnresponsive(JNIEnv* env,
                            const JavaParamRef<jclass>& clazz,
                            const JavaParamRef<jobject>& java_web_contents) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableHungRendererInfoBar)) {
    return;
  }

  content::WebContents* web_contents =
        content::WebContents::FromJavaWebContents(java_web_contents);
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  DCHECK(!FindHungRendererInfoBar(infobar_service));
  HungRendererInfoBarDelegate::Create(infobar_service,
                                      web_contents->GetRenderProcessHost());
}

void OnRendererResponsive(JNIEnv* env,
                          const JavaParamRef<jclass>& clazz,
                          const JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
          content::WebContents::FromJavaWebContents(java_web_contents);
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  infobars::InfoBar* hung_renderer_infobar =
      FindHungRendererInfoBar(infobar_service);
  if (!hung_renderer_infobar)
    return;

  hung_renderer_infobar->delegate()
      ->AsHungRendererInfoBarDelegate()
      ->OnRendererResponsive();
  infobar_service->RemoveInfoBar(hung_renderer_infobar);
}

jboolean IsCapturingAudio(JNIEnv* env,
                          const JavaParamRef<jclass>& clazz,
                          const JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  scoped_refptr<MediaStreamCaptureIndicator> indicator =
      MediaCaptureDevicesDispatcher::GetInstance()->
          GetMediaStreamCaptureIndicator();
  return indicator->IsCapturingAudio(web_contents);
}

jboolean IsCapturingVideo(JNIEnv* env,
                          const JavaParamRef<jclass>& clazz,
                          const JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  scoped_refptr<MediaStreamCaptureIndicator> indicator =
      MediaCaptureDevicesDispatcher::GetInstance()->
          GetMediaStreamCaptureIndicator();
  return indicator->IsCapturingVideo(web_contents);
}
