// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_DIALOG_DELEGATE_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/media/webrtc/media_stream_devices_controller.h"
#include "chrome/browser/permissions/permission_prompt_android.h"
#include "chrome/browser/permissions/permission_util.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_contents_observer.h"

using base::android::JavaParamRef;

namespace content {
class WebContents;
}
class GURL;
class PermissionInfoBarDelegate;
class Profile;
class TabAndroid;

// Delegate class for displaying a permission prompt as a modal dialog. Used as
// the native to Java interface to allow Java to communicate the user's
// decision.
//
// This class owns a PermissionInfoBarDelegate if the PermissionRequestManager
// is disabled and points to a PermissionPromptAndroid if it's enabled. When the
// PermissionRequestManager is enabled by default, we can remove the code path
// for using a PermissionInfoBarDelegate. This is tracked in crbug.com/606138.
class PermissionDialogDelegate : public content::WebContentsObserver {
 public:
  using PermissionSetCallback = base::Callback<void(bool, PermissionAction)>;

  // The interface for creating a modal dialog when the PermissionRequestManager
  // is enabled.
  static void Create(content::WebContents* web_contents,
                     PermissionPromptAndroid* permission_prompt);

  // The interface for creating a modal dialog when the PermissionRequestManager
  // is disabled, i.e. we're using the PermissionQueueController.
  static void Create(content::WebContents* web_contents,
                     ContentSettingsType type,
                     const GURL& requesting_frame,
                     bool user_gesture,
                     Profile* profile,
                     const PermissionSetCallback& callback);

  // Creates a modal dialog for a media stream permission request.
  // TODO(dominickn): remove this when media stream requests are eventually
  // folded in with other permission requests.
  static void CreateMediaStreamDialog(
      content::WebContents* web_contents,
      bool user_gesture,
      std::unique_ptr<MediaStreamDevicesController::Request> request);

  // Returns true if we should show the user a modal permission prompt rather
  // than an infobar.
  static bool ShouldShowDialog(bool has_user_gesture);

  // JNI methods.
  void Accept(JNIEnv* env, const JavaParamRef<jobject>& obj, jboolean persist);
  void Cancel(JNIEnv* env, const JavaParamRef<jobject>& obj, jboolean persist);
  void Dismissed(JNIEnv* env, const JavaParamRef<jobject>& obj);
  void LinkClicked(JNIEnv* env, const JavaParamRef<jobject>& obj);

  // Frees this object and the wrapped PermissionInfoBarDelegate. Called from
  // Java once the permission dialog has been responded to.
  void Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj);

 private:
  PermissionDialogDelegate(
      TabAndroid* tab,
      std::unique_ptr<PermissionInfoBarDelegate> infobar_delegate,
      PermissionPromptAndroid* permission_prompt);
  ~PermissionDialogDelegate() override;

  void CreateJavaDelegate(JNIEnv* env);

  // On navigation or page destruction, hide the dialog.
  void DismissDialog();

  // WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;

  base::android::ScopedJavaGlobalRef<jobject> j_delegate_;

  TabAndroid* tab_;

  // TODO(timloh): Remove this when the refactoring is finished and we can
  // delete the PermissionQueueController.
  std::unique_ptr<PermissionInfoBarDelegate> infobar_delegate_;

  // The PermissionPromptAndroid is deleted when either the dialog is resolved
  // or the tab is navigated/closed. We close the prompt on DidFinishNavigation
  // and WebContentsDestroyed, so it should always be safe to use this pointer.
  PermissionPromptAndroid* permission_prompt_;

  DISALLOW_COPY_AND_ASSIGN(PermissionDialogDelegate);
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_DIALOG_DELEGATE_H_
