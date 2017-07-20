// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_dialog_delegate.h"

#include <utility>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/geolocation/geolocation_infobar_delegate_android.h"
#include "chrome/browser/media/midi_permission_infobar_delegate_android.h"
#include "chrome/browser/media/protected_media_identifier_infobar_delegate_android.h"
#include "chrome/browser/media/webrtc/media_stream_infobar_delegate_android.h"
#include "chrome/browser/notifications/notification_permission_infobar_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "jni/PermissionDialogController_jni.h"
#include "jni/PermissionDialogDelegate_jni.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"

using base::android::ConvertUTF16ToJavaString;

namespace {

// Key for querying variations for whether a modal should require a gesture.
const char kModalParamsUserGestureKey[] = "require_gesture";

}

// static
void PermissionDialogDelegate::Create(
    content::WebContents* web_contents,
    PermissionPromptAndroid* permission_prompt) {
  DCHECK(web_contents);

  // If we don't have a tab, just act as though the prompt was dismissed.
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  if (!tab) {
    permission_prompt->Closing();
    return;
  }

  // Dispatch the dialog to Java, which manages the lifetime of this object.
  new PermissionDialogDelegate(tab, /*infobar_delegate=*/nullptr,
                               permission_prompt);
}

// static
void PermissionDialogDelegate::Create(content::WebContents* web_contents,
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
      tab,
      PermissionInfoBarDelegate::CreateDelegate(
          type, requesting_frame, user_gesture, profile, callback),
      /*permission_prompt=*/nullptr);
}

// static
void PermissionDialogDelegate::CreateMediaStreamDialog(
    content::WebContents* web_contents,
    bool user_gesture,
    std::unique_ptr<MediaStreamDevicesController::Request> request) {
  DCHECK(web_contents);

  // If we don't have a tab, just act as though the prompt was dismissed.
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  if (!tab) {
    request->Cancelled();
    return;
  }

  // Called this way because the infobar delegate has a private destructor.
  std::unique_ptr<PermissionInfoBarDelegate> infobar_delegate;
  infobar_delegate.reset(new MediaStreamInfoBarDelegateAndroid(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()),
      user_gesture, std::move(request)));

  // Dispatch the dialog to Java, which manages the lifetime of this object.
  new PermissionDialogDelegate(tab, std::move(infobar_delegate),
                               /*permission_prompt=*/nullptr);
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

void PermissionDialogDelegate::CreateJavaDelegate(JNIEnv* env) {
  base::android::ScopedJavaLocalRef<jstring> primaryButtonText =
      ConvertUTF16ToJavaString(env,
                               l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW));
  base::android::ScopedJavaLocalRef<jstring> secondaryButtonText =
      ConvertUTF16ToJavaString(env,
                               l10n_util::GetStringUTF16(IDS_PERMISSION_DENY));

  if (infobar_delegate_) {
    std::vector<int> content_settings_types{
        infobar_delegate_->content_settings_types()};

    j_delegate_.Reset(Java_PermissionDialogDelegate_create(
        env, reinterpret_cast<uintptr_t>(this), tab_->GetJavaObject(),
        base::android::ToJavaIntArray(env, content_settings_types).obj(),
        ResourceMapper::MapFromChromiumId(infobar_delegate_->GetIconId()),
        ConvertUTF16ToJavaString(env, infobar_delegate_->GetMessageText()),
        ConvertUTF16ToJavaString(env, infobar_delegate_->GetLinkText()),
        primaryButtonText, secondaryButtonText,
        infobar_delegate_->ShouldShowPersistenceToggle()));
    return;
  }

  std::vector<int> content_settings_types;
  for (size_t i = 0; i < permission_prompt_->PermissionCount(); ++i) {
    content_settings_types.push_back(
        permission_prompt_->GetContentSettingType(i));
  }

  j_delegate_.Reset(Java_PermissionDialogDelegate_create(
      env, reinterpret_cast<uintptr_t>(this), tab_->GetJavaObject(),
      base::android::ToJavaIntArray(env, content_settings_types).obj(),
      ResourceMapper::MapFromChromiumId(permission_prompt_->GetIconId()),
      ConvertUTF16ToJavaString(env, permission_prompt_->GetMessageText()),
      ConvertUTF16ToJavaString(env, permission_prompt_->GetLinkText()),
      primaryButtonText, secondaryButtonText,
      permission_prompt_->ShouldShowPersistenceToggle()));
}

void PermissionDialogDelegate::Accept(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj,
                                      jboolean persist) {
  if (infobar_delegate_) {
    if (infobar_delegate_->ShouldShowPersistenceToggle())
      infobar_delegate_->set_persist(persist);
    infobar_delegate_->Accept();
    return;
  }

  if (permission_prompt_->ShouldShowPersistenceToggle())
    permission_prompt_->TogglePersist(persist);
  permission_prompt_->Accept();
}

void PermissionDialogDelegate::Cancel(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj,
                                      jboolean persist) {
  if (infobar_delegate_) {
    if (infobar_delegate_->ShouldShowPersistenceToggle())
      infobar_delegate_->set_persist(persist);
    infobar_delegate_->Cancel();
    return;
  }

  if (permission_prompt_->ShouldShowPersistenceToggle())
    permission_prompt_->TogglePersist(persist);
  permission_prompt_->Deny();
}

void PermissionDialogDelegate::Dismissed(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj) {
  if (infobar_delegate_) {
    infobar_delegate_->InfoBarDismissed();
    return;
  }

  permission_prompt_->Closing();
}

void PermissionDialogDelegate::LinkClicked(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj) {
  // Don't call delegate_->LinkClicked() because that relies on having an
  // InfoBarService as an owner() to open the link. That will fail since the
  // wrapped delegate has no owner (it hasn't been added as an infobar).
  if (tab_->web_contents()) {
    GURL linkURL = infobar_delegate_ ? infobar_delegate_->GetLinkURL()
                                     : permission_prompt_->GetLinkURL();
    tab_->web_contents()->OpenURL(content::OpenURLParams(
        linkURL, content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui::PAGE_TRANSITION_LINK, false));
  }
}

void PermissionDialogDelegate::Destroy(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  delete this;
}

PermissionDialogDelegate::PermissionDialogDelegate(
    TabAndroid* tab,
    std::unique_ptr<PermissionInfoBarDelegate> infobar_delegate,
    PermissionPromptAndroid* permission_prompt)
    : content::WebContentsObserver(tab->web_contents()),
      tab_(tab),
      infobar_delegate_(std::move(infobar_delegate)),
      permission_prompt_(permission_prompt) {
  DCHECK(tab_);
  // Only one of the PermissionPromptAndroid and PermissionInfoBarDelegate is
  // used, depending on whether the PermissionRequestManager is enabled or not.
  DCHECK(!!permission_prompt_ ^ !!infobar_delegate_);

  // Create our Java counterpart, which manages our lifetime.
  JNIEnv* env = base::android::AttachCurrentThread();
  CreateJavaDelegate(env);

  // Send the Java delegate to the Java PermissionDialogController for display.
  // The controller takes over lifetime management; when the Java delegate is no
  // longer needed it will in turn free the native delegate.
  Java_PermissionDialogController_createDialog(env, j_delegate_.obj());
}

PermissionDialogDelegate::~PermissionDialogDelegate() {}

void PermissionDialogDelegate::DismissDialog() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PermissionDialogDelegate_dismissFromNative(env, j_delegate_.obj());
}

void PermissionDialogDelegate::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  DismissDialog();
}

void PermissionDialogDelegate::WebContentsDestroyed() {
  DismissDialog();
}
