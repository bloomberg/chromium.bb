// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ANDROID_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_MEDIA_ANDROID_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/media/router/media_router_dialog_controller.h"
#include "content/public/browser/web_contents_user_data.h"

namespace media_router {

// Android implementation of the MediaRouterDialogController.
class MediaRouterDialogControllerAndroid
    : public content::WebContentsUserData<MediaRouterDialogControllerAndroid>,
      public MediaRouterDialogController {
 public:
  ~MediaRouterDialogControllerAndroid() override;

  static bool Register(JNIEnv* env);

  static MediaRouterDialogControllerAndroid* GetOrCreateForWebContents(
      content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<MediaRouterDialogControllerAndroid>;

  // Use MediaRouterDialogControllerAndroid::CreateForWebContents() to create an
  // instance.
  explicit MediaRouterDialogControllerAndroid(
      content::WebContents* web_contents);

  // MediaRouterDialogController:
  void CreateMediaRouterDialog() override;
  void CloseMediaRouterDialog() override;
  bool IsShowingMediaRouterDialog() const override;

  base::android::ScopedJavaGlobalRef<jobject> java_dialog_controller_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterDialogControllerAndroid);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ANDROID_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_ANDROID_H_
