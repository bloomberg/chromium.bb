// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_android.h"

#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/password_manager/password_manager_delegate_impl.h"
#include "chrome/browser/prerender/prerender_tab_helper.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ssl/ssl_tab_helper.h"
#include "chrome/browser/translate/translate_tab_helper.h"
#include "chrome/browser/ui/alternate_error_tab_observer.h"
#include "chrome/browser/ui/android/window_android_helper.h"
#include "chrome/browser/ui/autofill/tab_autofill_manager_delegate.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/browser_tab_contents.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/toolbar/toolbar_model_impl.h"
#include "chrome/browser/view_type_utils.h"
#include "components/autofill/browser/autofill_external_delegate.h"
#include "components/autofill/browser/autofill_manager.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/web_contents.h"
#include "jni/TabBase_jni.h"

using content::WebContents;

namespace {

const char kTabHelpersInitializedUserDataKey[] =
    "TabAndroidTabHelpersInitialized";

}  // namespace

void BrowserTabContents::AttachTabHelpers(WebContents* contents) {
  // If already initialized, nothing to be done.
  base::SupportsUserData::Data* initialization_tag =
      contents->GetUserData(&kTabHelpersInitializedUserDataKey);
  if (initialization_tag)
    return;

  // Mark as initialized.
  contents->SetUserData(&kTabHelpersInitializedUserDataKey,
                            new base::SupportsUserData::Data());

  // Set the view type.
  chrome::SetViewType(contents, chrome::VIEW_TYPE_TAB_CONTENTS);

  // SessionTabHelper comes first because it sets up the tab ID, and other
  // helpers may rely on that.
  SessionTabHelper::CreateForWebContents(contents);

  AlternateErrorPageTabObserver::CreateForWebContents(contents);
  autofill::TabAutofillManagerDelegate::CreateForWebContents(contents);
  AutofillManager::CreateForWebContentsAndDelegate(
      contents,
      autofill::TabAutofillManagerDelegate::FromWebContents(contents));
  AutofillExternalDelegate::CreateForWebContentsAndManager(
      contents, AutofillManager::FromWebContents(contents));
  AutofillManager::FromWebContents(contents)->SetExternalDelegate(
      AutofillExternalDelegate::FromWebContents(contents));
  BlockedContentTabHelper::CreateForWebContents(contents);
  BookmarkTabHelper::CreateForWebContents(contents);
  CoreTabHelper::CreateForWebContents(contents);
  extensions::TabHelper::CreateForWebContents(contents);
  FaviconTabHelper::CreateForWebContents(contents);
  FindTabHelper::CreateForWebContents(contents);
  HistoryTabHelper::CreateForWebContents(contents);
  InfoBarService::CreateForWebContents(contents);
  PasswordManagerDelegateImpl::CreateForWebContents(contents);
  PasswordManager::CreateForWebContentsAndDelegate(
      contents, PasswordManagerDelegateImpl::FromWebContents(contents));
  PrefsTabHelper::CreateForWebContents(contents);
  prerender::PrerenderTabHelper::CreateForWebContents(contents);
  SSLTabHelper::CreateForWebContents(contents);
  TabContentsSyncedTabDelegate::CreateForWebContents(contents);
  TabSpecificContentSettings::CreateForWebContents(contents);
  TranslateTabHelper::CreateForWebContents(contents);
  WindowAndroidHelper::CreateForWebContents(contents);
}

void TabAndroid::InitTabHelpers(WebContents* contents) {
  BrowserTabContents::AttachTabHelpers(contents);
}

WebContents* TabAndroid::InitWebContentsFromView(JNIEnv* env,
                                                 jobject content_view) {
  content::ContentViewCore* content_view_core =
      content::ContentViewCore::GetNativeContentViewCore(env, content_view);
  DCHECK(content_view_core);
  WebContents* web_contents = content_view_core->GetWebContents();
  DCHECK(web_contents);
  InitTabHelpers(web_contents);
  return web_contents;
}

TabAndroid::TabAndroid(JNIEnv* env, jobject obj)
    : tab_id_(-1),
      weak_java_tab_(env, obj) {
  Java_TabBase_setNativePtr(env, obj, reinterpret_cast<jint>(this));
}

TabAndroid::~TabAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_tab_.get(env);
  if (obj.is_null())
    return;

  Java_TabBase_destroyBase(env, obj.obj());
}

content::WebContents* TabAndroid::GetWebContents() {
  return NULL;
}

ToolbarModel::SecurityLevel TabAndroid::GetSecurityLevel() {
  return ToolbarModelImpl::GetSecurityLevelForWebContents(GetWebContents());
}

void TabAndroid::RunExternalProtocolDialog(const GURL& url) {
}

bool TabAndroid::RegisterTabAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
