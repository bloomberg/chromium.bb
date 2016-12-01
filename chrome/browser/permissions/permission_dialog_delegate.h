// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_DIALOG_DELEGATE_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/permissions/permission_util.h"
#include "content/public/browser/permission_type.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace content {
class WebContents;
}

class MediaStreamDevicesController;
class GURL;
class PermissionInfoBarDelegate;
class Profile;
class TabAndroid;

// Delegate class for displaying a permission prompt as a modal dialog. Used as
// the native to Java interface to allow Java to communicate the user's
// decision.
//
// This class currently wraps a PermissionInfoBarDelegate. Future refactoring
// will consolidate PermissionInfoBarDelegate and its subclasses together into
// GroupedPermissionInfoBarDelegate, which will then source all of its data from
// an underlying PermissionPromptAndroid object. At that time, this class will
// also change to wrap a PermissionPromptAndroid.
class PermissionDialogDelegate {
 public:
  using PermissionSetCallback = base::Callback<void(bool, PermissionAction)>;

  // Creates a modal dialog for |type|.
  static void Create(content::WebContents* web_contents,
                     content::PermissionType type,
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
      std::unique_ptr<MediaStreamDevicesController> controller);

  // Returns true if we should show the user a modal permission prompt rather
  // than an infobar.
  static bool ShouldShowDialog(bool has_user_gesture);

  // JNI methods.
  static bool RegisterPermissionDialogDelegate(JNIEnv* env);
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
      std::unique_ptr<PermissionInfoBarDelegate> infobar_delegate_);
  ~PermissionDialogDelegate();

  ScopedJavaLocalRef<jobject> CreateJavaDelegate(JNIEnv* env);

  TabAndroid* tab_;

  // The InfoBarDelegate which this class is wrapping.
  // TODO(dominickn,lshang) replace this with PermissionPromptAndroid as the
  // permission prompt refactoring continues.
  std::unique_ptr<PermissionInfoBarDelegate> infobar_delegate_;

  DISALLOW_COPY_AND_ASSIGN(PermissionDialogDelegate);
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_DIALOG_DELEGATE_H_
