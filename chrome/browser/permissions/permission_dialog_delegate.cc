// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_dialog_delegate.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/geolocation/geolocation_infobar_delegate_android.h"
#include "chrome/browser/media/midi_permission_infobar_delegate_android.h"
#include "chrome/browser/media/protected_media_identifier_infobar_delegate_android.h"
#include "chrome/browser/media/webrtc/media_stream_devices_controller.h"
#include "chrome/browser/media/webrtc/media_stream_infobar_delegate_android.h"
#include "chrome/browser/notifications/notification_permission_infobar_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/web_contents.h"
#include "jni/PermissionDialogController_jni.h"
#include "jni/PermissionDialogDelegate_jni.h"
#include "ui/android/window_android.h"
#include "ui/base/window_open_disposition.h"

using base::android::ConvertUTF16ToJavaString;

namespace {

// Key for querying variations for whether a modal should require a gesture.
const char kModalParamsUserGestureKey[] = "require_gesture";

}

// static
void PermissionDialogDelegate::Create(
    content::WebContents* web_contents,
    ContentSettingsType type,
    const GURL& requesting_frame,
    bool user_gesture,
    Profile* profile,
    const PermissionSetCallback& callback) {
  DCHECK(web_contents);

  // If we don't have a tab, just act as though the prompt was dismissed.
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  if (!tab) {
    callback.Run(false, PermissionAction::DISMISSED);
    return;
  }

  // Dispatch the dialog to Java, which manages the lifetime of this object.
  new PermissionDialogDelegate(
      tab, PermissionInfoBarDelegate::CreateDelegate(
               type, requesting_frame, user_gesture, profile, callback));
}

// static
void PermissionDialogDelegate::CreateMediaStreamDialog(
    content::WebContents* web_contents,
    bool user_gesture,
    std::unique_ptr<MediaStreamDevicesController> controller) {
  DCHECK(web_contents);

  // If we don't have a tab, just act as though the prompt was dismissed.
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  if (!tab) {
    controller->Cancelled();
    return;
  }

  // Called this way because the infobar delegate has a private destructor.
  std::unique_ptr<PermissionInfoBarDelegate> infobar_delegate;
  infobar_delegate.reset(new MediaStreamInfoBarDelegateAndroid(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()),
      user_gesture,
      std::move(controller)));

  // Dispatch the dialog to Java, which manages the lifetime of this object.
  new PermissionDialogDelegate(tab, std::move(infobar_delegate));
}

// static
bool PermissionDialogDelegate::ShouldShowDialog(bool has_user_gesture) {
  if (!base::FeatureList::IsEnabled(features::kModalPermissionPrompts))
    return false;

  // Only use modals when the prompt is triggered by a user gesture, unless the
  // kModalParamsUserGestureKey is set to false.
  std::string require_gesture = variations::GetVariationParamValueByFeature(
      features::kModalPermissionPrompts, kModalParamsUserGestureKey);
  if (require_gesture == "false")
    return true;
  return has_user_gesture;
}

// static
bool PermissionDialogDelegate::RegisterPermissionDialogDelegate(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

ScopedJavaLocalRef<jobject> PermissionDialogDelegate::CreateJavaDelegate(
    JNIEnv* env) {
  std::vector<int> content_settings_types{
      infobar_delegate_->content_settings_types()};

  return Java_PermissionDialogDelegate_create(
      env, reinterpret_cast<uintptr_t>(this),
      tab_->GetJavaObject(),
      base::android::ToJavaIntArray(env, content_settings_types).obj(),
      ResourceMapper::MapFromChromiumId(infobar_delegate_->GetIconId()),
      ConvertUTF16ToJavaString(env, infobar_delegate_->GetMessageText()),
      ConvertUTF16ToJavaString(env, infobar_delegate_->GetLinkText()),
      ConvertUTF16ToJavaString(env, infobar_delegate_->GetButtonLabel(
                                        PermissionInfoBarDelegate::BUTTON_OK)),
      ConvertUTF16ToJavaString(env,
                               infobar_delegate_->GetButtonLabel(
                                   PermissionInfoBarDelegate::BUTTON_CANCEL)),
      infobar_delegate_->ShouldShowPersistenceToggle());
}

void PermissionDialogDelegate::Accept(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj,
                                      jboolean persist) {
  if (infobar_delegate_->ShouldShowPersistenceToggle())
    infobar_delegate_->set_persist(persist);
  infobar_delegate_->Accept();
}

void PermissionDialogDelegate::Cancel(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj,
                                      jboolean persist) {
  if (infobar_delegate_->ShouldShowPersistenceToggle())
    infobar_delegate_->set_persist(persist);
  infobar_delegate_->Cancel();
}

void PermissionDialogDelegate::Dismissed(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj) {
  infobar_delegate_->InfoBarDismissed();
}

void PermissionDialogDelegate::LinkClicked(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj) {
  // Don't call delegate_->LinkClicked() because that relies on having an
  // InfoBarService as an owner() to open the link. That will fail since the
  // wrapped delegate has no owner (it hasn't been added as an infobar).
  if (tab_->web_contents()) {
    tab_->web_contents()->OpenURL(content::OpenURLParams(
        infobar_delegate_->GetLinkURL(), content::Referrer(),
        WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
        false));
  }
}

void PermissionDialogDelegate::Destroy(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  delete this;
}

PermissionDialogDelegate::PermissionDialogDelegate(
    TabAndroid* tab,
    std::unique_ptr<PermissionInfoBarDelegate> infobar_delegate)
    : tab_(tab), infobar_delegate_(std::move(infobar_delegate)) {
  DCHECK(tab_);
  DCHECK(infobar_delegate_);

  // Create our Java counterpart, which manages our lifetime.
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> j_delegate =
      CreateJavaDelegate(env);

  // Send the Java delegate to the Java PermissionDialogController for display.
  // The controller takes over lifetime management; when the Java delegate is no
  // longer needed it will in turn free the native delegate.
  Java_PermissionDialogController_createDialog(env, j_delegate.obj());
}

PermissionDialogDelegate::~PermissionDialogDelegate() {}
