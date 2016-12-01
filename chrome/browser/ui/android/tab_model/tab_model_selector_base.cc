// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model_selector_base.h"

#include "base/android/jni_android.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "content/public/browser/web_contents.h"
#include "jni/TabModelSelectorBase_jni.h"

using base::android::JavaParamRef;

static void OnActiveTabChanged(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& java_old_web_contents,
    const JavaParamRef<jobject>& java_new_web_contents) {
  // Check if PermissionRequestManager is reused on Android to manage infobars.
  // This is currently behind a switch flag.
  if (!PermissionRequestManager::IsEnabled())
    return;

  // Visible permission infobars will get hidden and shown when switching tabs
  // by the infobar infrastructure, but we do this explicitly to create/destroy
  // PermissionPromptAndroid for old and new tabs. This also keeps the codepath
  // consistent across different platforms.
  content::WebContents* old_contents =
      content::WebContents::FromJavaWebContents(java_old_web_contents);
  content::WebContents* new_contents =
      content::WebContents::FromJavaWebContents(java_new_web_contents);
  DCHECK(new_contents);
  if (old_contents)
    PermissionRequestManager::FromWebContents(old_contents)->HideBubble();

  if (new_contents) {
    PermissionRequestManager::FromWebContents(new_contents)
        ->DisplayPendingRequests();
  }
}

// Register native methods
bool RegisterTabModelSelectorBase(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
