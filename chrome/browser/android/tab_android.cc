// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_android.h"

#include "base/android/jni_android.h"
#include "chrome/browser/android/chrome_web_contents_delegate_android.h"
#include "chrome/browser/android/webapps/single_tab_mode_tab_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/net/net_error_tab_helper.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/password_manager/password_manager_delegate_impl.h"
#include "chrome/browser/prerender/prerender_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ssl/ssl_tab_helper.h"
#include "chrome/browser/sync/glue/synced_tab_delegate_android.h"
#include "chrome/browser/translate/translate_tab_helper.h"
#include "chrome/browser/ui/alternate_error_tab_observer.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/window_android_helper.h"
#include "chrome/browser/ui/autofill/tab_autofill_manager_delegate.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"
#include "chrome/browser/ui/blocked_content/popup_blocker_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/browser_tab_contents.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/toolbar/toolbar_model_impl.h"
#include "components/autofill/content/browser/autofill_driver_impl.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/view_type_utils.h"
#include "jni/TabBase_jni.h"

namespace {

const char kTabHelpersInitializedUserDataKey[] =
    "TabAndroidTabHelpersInitialized";

}  // namespace

void BrowserTabContents::AttachTabHelpers(content::WebContents* contents) {
  // If already initialized, nothing to be done.
  base::SupportsUserData::Data* initialization_tag =
      contents->GetUserData(&kTabHelpersInitializedUserDataKey);
  if (initialization_tag)
    return;

  // Mark as initialized.
  contents->SetUserData(&kTabHelpersInitializedUserDataKey,
                            new base::SupportsUserData::Data());

  // Set the view type.
  extensions::SetViewType(contents, extensions::VIEW_TYPE_TAB_CONTENTS);

  // SessionTabHelper comes first because it sets up the tab ID, and other
  // helpers may rely on that.
  SessionTabHelper::CreateForWebContents(contents);

  AlternateErrorPageTabObserver::CreateForWebContents(contents);
  autofill::TabAutofillManagerDelegate::CreateForWebContents(contents);
  autofill::AutofillDriverImpl::CreateForWebContentsAndDelegate(
      contents,
      autofill::TabAutofillManagerDelegate::FromWebContents(contents),
      g_browser_process->GetApplicationLocale(),
      autofill::AutofillManager::ENABLE_AUTOFILL_DOWNLOAD_MANAGER);
  BlockedContentTabHelper::CreateForWebContents(contents);
  BookmarkTabHelper::CreateForWebContents(contents);
  CoreTabHelper::CreateForWebContents(contents);
  extensions::TabHelper::CreateForWebContents(contents);
  FaviconTabHelper::CreateForWebContents(contents);
  FindTabHelper::CreateForWebContents(contents);
  HistoryTabHelper::CreateForWebContents(contents);
  InfoBarService::CreateForWebContents(contents);
  chrome_browser_net::NetErrorTabHelper::CreateForWebContents(contents);
  PasswordManagerDelegateImpl::CreateForWebContents(contents);
  PasswordManager::CreateForWebContentsAndDelegate(
      contents, PasswordManagerDelegateImpl::FromWebContents(contents));
  PopupBlockerTabHelper::CreateForWebContents(contents);
  PrefsTabHelper::CreateForWebContents(contents);
  prerender::PrerenderTabHelper::CreateForWebContentsWithPasswordManager(
      contents, PasswordManager::FromWebContents(contents));
  SingleTabModeTabHelper::CreateForWebContents(contents);
  SSLTabHelper::CreateForWebContents(contents);
  TabSpecificContentSettings::CreateForWebContents(contents);
  TranslateTabHelper::CreateForWebContents(contents);
  WindowAndroidHelper::CreateForWebContents(contents);
}

// TODO(dtrainor): Refactor so we do not need this method.
void TabAndroid::InitTabHelpers(content::WebContents* contents) {
  BrowserTabContents::AttachTabHelpers(contents);
}

TabAndroid* TabAndroid::FromWebContents(content::WebContents* web_contents) {
  CoreTabHelper* core_tab_helper = CoreTabHelper::FromWebContents(web_contents);
  if (!core_tab_helper)
    return NULL;

  CoreTabHelperDelegate* core_delegate = core_tab_helper->delegate();
  if (!core_delegate)
    return NULL;

  return static_cast<TabAndroid*>(core_delegate);
}

TabAndroid* TabAndroid::GetNativeTab(JNIEnv* env, jobject obj) {
  return reinterpret_cast<TabAndroid*>(Java_TabBase_getNativePtr(env, obj));
}

TabAndroid::TabAndroid(JNIEnv* env, jobject obj)
    : weak_java_tab_(env, obj),
      session_tab_id_(),
      android_tab_id_(-1),
      synced_tab_delegate_(new browser_sync::SyncedTabDelegateAndroid(this)) {
  Java_TabBase_setNativePtr(env, obj, reinterpret_cast<jint>(this));
}

TabAndroid::~TabAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_tab_.get(env);
  if (obj.is_null())
    return;

  Java_TabBase_clearNativePtr(env, obj.obj());
}

content::ContentViewCore* TabAndroid::GetContentViewCore() const {
  if (!web_contents())
    return NULL;

  return content::ContentViewCore::FromWebContents(web_contents());
}

Profile* TabAndroid::GetProfile() const {
  if (!web_contents())
    return NULL;

  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

browser_sync::SyncedTabDelegate* TabAndroid::GetSyncedTabDelegate() const {
  return synced_tab_delegate_.get();
}

void TabAndroid::RunExternalProtocolDialog(const GURL& url) {
}

void TabAndroid::SwapTabContents(content::WebContents* old_contents,
                                 content::WebContents* new_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TabBase_swapWebContents(
      env,
      weak_java_tab_.get(env).obj(),
      reinterpret_cast<jint>(new_contents));
}

void TabAndroid::Observe(int type,
                         const content::NotificationSource& source,
                         const content::NotificationDetails& details) {
  JNIEnv* env = base::android::AttachCurrentThread();
  switch (type) {
    case chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED: {
      TabSpecificContentSettings* settings =
          TabSpecificContentSettings::FromWebContents(web_contents());
      if (!settings->IsBlockageIndicated(CONTENT_SETTINGS_TYPE_POPUPS)) {
        // TODO(dfalcantara): Create an InfoBarDelegate to keep the
        // PopupBlockedInfoBar logic native-side instead of straddling the JNI
        // boundary.
        int num_popups = 0;
        PopupBlockerTabHelper* popup_blocker_helper =
            PopupBlockerTabHelper::FromWebContents(web_contents());
        if (popup_blocker_helper)
          num_popups = popup_blocker_helper->GetBlockedPopupsCount();

        Java_TabBase_onBlockedPopupsStateChanged(env,
                                                 weak_java_tab_.get(env).obj(),
                                                 num_popups);
        settings->SetBlockageHasBeenIndicated(CONTENT_SETTINGS_TYPE_POPUPS);
      }
      break;
    }
    case chrome::NOTIFICATION_FAVICON_UPDATED:
      Java_TabBase_onFaviconUpdated(env, weak_java_tab_.get(env).obj());
      break;
    default:
      NOTREACHED() << "Unexpected notification " << type;
      break;
  }
}

void TabAndroid::InitWebContents(JNIEnv* env,
                                 jobject obj,
                                 jint tab_id,
                                 jboolean incognito,
                                 jobject jcontent_view_core,
                                 jobject jweb_contents_delegate) {
  android_tab_id_ = tab_id;

  content::ContentViewCore* content_view_core =
      content::ContentViewCore::GetNativeContentViewCore(env,
                                                         jcontent_view_core);
  DCHECK(content_view_core);
  DCHECK(content_view_core->GetWebContents());

  web_contents_.reset(content_view_core->GetWebContents());
  InitTabHelpers(web_contents_.get());

  session_tab_id_.set_id(
      SessionTabHelper::FromWebContents(web_contents())->session_id().id());
  WindowAndroidHelper::FromWebContents(web_contents())->
      SetWindowAndroid(content_view_core->GetWindowAndroid());
  CoreTabHelper::FromWebContents(web_contents())->set_delegate(this);
  web_contents_delegate_.reset(
      new chrome::android::ChromeWebContentsDelegateAndroid(
          env, jweb_contents_delegate));
  web_contents_delegate_->LoadProgressChanged(web_contents(), 0);
  web_contents()->SetDelegate(web_contents_delegate_.get());

  notification_registrar_.Add(
      this,
      chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED,
      content::Source<content::WebContents>(web_contents()));
  notification_registrar_.Add(
      this,
      chrome::NOTIFICATION_FAVICON_UPDATED,
      content::Source<content::WebContents>(web_contents()));

  synced_tab_delegate_->SetWebContents(web_contents());

  // Set the window ID if there is a valid TabModel.
  TabModel* model = TabModelList::GetTabModelWithProfile(GetProfile());
  if (model) {
    SessionID window_id;
    window_id.set_id(model->GetSessionId());

    SessionTabHelper* session_tab_helper =
        SessionTabHelper::FromWebContents(web_contents());
    session_tab_helper->SetWindowID(window_id);
  }

  // Verify that the WebContents this tab represents matches the expected
  // off the record state.
  CHECK_EQ(GetProfile()->IsOffTheRecord(), incognito);
}

void TabAndroid::DestroyWebContents(JNIEnv* env,
                                    jobject obj,
                                    jboolean delete_native) {
  DCHECK(web_contents());

  notification_registrar_.Remove(
      this,
      chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED,
      content::Source<content::WebContents>(web_contents()));
  notification_registrar_.Remove(
      this,
      chrome::NOTIFICATION_FAVICON_UPDATED,
      content::Source<content::WebContents>(web_contents()));

  web_contents()->SetDelegate(NULL);

  if (delete_native) {
    web_contents_.reset();
    synced_tab_delegate_->ResetWebContents();
  } else {
    // Release the WebContents so it does not get deleted by the scoped_ptr.
    ignore_result(web_contents_.release());
  }
}

base::android::ScopedJavaLocalRef<jobject> TabAndroid::GetProfileAndroid(
    JNIEnv* env,
    jobject obj) {
  Profile* profile = GetProfile();
  if (!profile)
    return base::android::ScopedJavaLocalRef<jobject>();
  ProfileAndroid* profile_android = ProfileAndroid::FromProfile(profile);
  if (!profile_android)
    return base::android::ScopedJavaLocalRef<jobject>();

  return profile_android->GetJavaObject();
}

void TabAndroid::LaunchBlockedPopups(JNIEnv* env, jobject obj) {
  PopupBlockerTabHelper* popup_blocker_helper =
      PopupBlockerTabHelper::FromWebContents(web_contents());
  DCHECK(popup_blocker_helper);
  std::map<int32, GURL> blocked_popups =
      popup_blocker_helper->GetBlockedPopupRequests();
  for (std::map<int32, GURL>::iterator it = blocked_popups.begin();
      it != blocked_popups.end();
      it++) {
    popup_blocker_helper->ShowBlockedPopup(it->first);
  }
}

ToolbarModel::SecurityLevel TabAndroid::GetSecurityLevel(JNIEnv* env,
                                                         jobject obj) {
  return ToolbarModelImpl::GetSecurityLevelForWebContents(web_contents());
}

bool TabAndroid::RegisterTabAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
