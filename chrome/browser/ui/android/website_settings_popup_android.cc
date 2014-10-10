// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/website_settings_popup_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/website_settings/website_settings.h"
#include "chrome/browser/ui/website_settings/website_settings_ui.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "jni/WebsiteSettingsPopup_jni.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;

// static
static jlong Init(JNIEnv* env,
                  jclass clazz,
                  jobject obj,
                  jobject java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);

  return reinterpret_cast<intptr_t>(
      new WebsiteSettingsPopupAndroid(env, obj, web_contents));
}

WebsiteSettingsPopupAndroid::WebsiteSettingsPopupAndroid(
    JNIEnv* env,
    jobject java_website_settings_pop,
    content::WebContents* web_contents) {
  // Important to use GetVisibleEntry to match what's showing in the omnibox.
  content::NavigationEntry* nav_entry =
      web_contents->GetController().GetVisibleEntry();
  if (nav_entry == NULL)
    return;

  url_ = nav_entry->GetURL();

  popup_jobject_.Reset(env, java_website_settings_pop);

  presenter_.reset(new WebsiteSettings(
      this,
      Profile::FromBrowserContext(web_contents->GetBrowserContext()),
      TabSpecificContentSettings::FromWebContents(web_contents),
      InfoBarService::FromWebContents(web_contents),
      nav_entry->GetURL(),
      nav_entry->GetSSL(),
      content::CertStore::GetInstance()));
}

WebsiteSettingsPopupAndroid::~WebsiteSettingsPopupAndroid() {}

void WebsiteSettingsPopupAndroid::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void WebsiteSettingsPopupAndroid::SetIdentityInfo(
    const IdentityInfo& identity_info) {
  JNIEnv* env = base::android::AttachCurrentThread();

  enum PageInfoConnectionType connection_type = CONNECTION_UNKNOWN;
  switch (identity_info.connection_status) {
    case WebsiteSettings::SITE_CONNECTION_STATUS_UNKNOWN:
      connection_type = CONNECTION_UNKNOWN;
      break;
    case WebsiteSettings::SITE_CONNECTION_STATUS_ENCRYPTED:
      connection_type = CONNECTION_ENCRYPTED;
      break;
    case WebsiteSettings::SITE_CONNECTION_STATUS_MIXED_CONTENT:
      connection_type = CONNECTION_MIXED_CONTENT;
      break;
    case WebsiteSettings::SITE_CONNECTION_STATUS_UNENCRYPTED:
      connection_type = CONNECTION_UNENCRYPTED;
      break;
    case WebsiteSettings::SITE_CONNECTION_STATUS_ENCRYPTED_ERROR:
      connection_type = CONNECTION_ENCRYPTED_ERROR;
      break;
    case WebsiteSettings::SITE_CONNECTION_STATUS_INTERNAL_PAGE:
      connection_type = CONNECTION_INTERNAL_PAGE;
      break;
    default:
      NOTREACHED();
      break;
  }

  Java_WebsiteSettingsPopup_setURLTitle(
      env,
      popup_jobject_.obj(),
      ConvertUTF8ToJavaString(env, url_.scheme()).obj(),
      ConvertUTF8ToJavaString(env, url_.host()).obj(),
      ConvertUTF8ToJavaString(env, url_.path()).obj(),
      static_cast<jint>(connection_type));

  Java_WebsiteSettingsPopup_setConnectionMessage(
      env,
      popup_jobject_.obj(),
      ConvertUTF16ToJavaString(
          env,
          l10n_util::GetStringUTF16(
              WebsiteSettingsUI::GetConnectionSummaryMessageID(
                  identity_info.connection_status))).obj());

  Java_WebsiteSettingsPopup_showDialog(env, popup_jobject_.obj());
}

void WebsiteSettingsPopupAndroid::SetCookieInfo(
    const CookieInfoList& cookie_info_list) {
  NOTIMPLEMENTED();
}

void WebsiteSettingsPopupAndroid::SetPermissionInfo(
    const PermissionInfoList& permission_info_list) {
  NOTIMPLEMENTED();
}

void WebsiteSettingsPopupAndroid::SetSelectedTab(
    WebsiteSettingsUI::TabId tab_id) {
  // There's no tab UI on Android - only connection info is shown.
  NOTIMPLEMENTED();
}

void WebsiteSettingsPopupAndroid::SetFirstVisit(
    const base::string16& first_visit) {
  NOTIMPLEMENTED();
}

// static
bool WebsiteSettingsPopupAndroid::RegisterWebsiteSettingsPopupAndroid(
    JNIEnv* env) {
  return RegisterNativesImpl(env);
}
