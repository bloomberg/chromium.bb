// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ANDROID_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_MEDIA_ANDROID_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/media/router/media_router_dialog_controller.h"
#include "chrome/browser/media/router/media_routes_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace media_router {

// Android implementation of the MediaRouterDialogController.
class MediaRouterDialogControllerAndroid
    : public content::WebContentsUserData<MediaRouterDialogControllerAndroid>,
      public MediaRouterDialogController,
      public MediaRoutesObserver {
 public:
  ~MediaRouterDialogControllerAndroid() override;

  static bool Register(JNIEnv* env);

  static MediaRouterDialogControllerAndroid* GetOrCreateForWebContents(
      content::WebContents* web_contents);

  // The methods called by the Java counterpart.

  // Notifies the controller that user has selected a sink with |jsink_id|.
  void OnSinkSelected(JNIEnv* env, jobject obj, jstring jsink_id);
  // Notifies the controller that user chose to close the route.
  void OnRouteClosed(JNIEnv* env, jobject obj, jstring jmedia_route_id);
  // Notifies the controller that the dialog has been closed without the user
  // taking any action (e.g. closing the route or selecting a sink).
  void OnDialogCancelled(JNIEnv* env, jobject obj);

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

  // MediaRoutesObserver:
  void OnRoutesUpdated(const std::vector<MediaRoute>& routes) override;

  void CancelPresentationRequest();

  base::android::ScopedJavaGlobalRef<jobject> java_dialog_controller_;

  // Null if no routes or more than one route exist. If there's only one route,
  // keeps a copy to determine if the route controller dialog needs to be shown
  // vs. the route chooser one.
  scoped_ptr<MediaRoute> single_existing_route_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterDialogControllerAndroid);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ANDROID_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_ANDROID_H_
