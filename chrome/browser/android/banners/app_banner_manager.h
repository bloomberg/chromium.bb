// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_MANAGER_H_
#define CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_MANAGER_H_

#include "base/android/jni_android.h"
#include "base/android/jni_helper.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/android/meta_tag_observer.h"

namespace content {
struct FrameNavigateParams;
struct LoadCommittedDetails;
}  // namespace content

/**
 * Manages when an app banner is created or dismissed.
 *
 * Hooks the wiring together for getting the data for a particular app.
 * Monitors at most one package at a time, and tracks the info for the
 * most recent app that was requested.  Any work in progress for other apps is
 * discarded.
 */
class AppBannerManager : public MetaTagObserver {
 public:
  AppBannerManager(JNIEnv* env, jobject obj);
  virtual ~AppBannerManager();

  // Destroys the AppBannerManager.
  void Destroy(JNIEnv* env, jobject obj);

  // Observes a new WebContents, if necessary.
  void ReplaceWebContents(JNIEnv* env,
                          jobject obj,
                          jobject jweb_contents);

  // WebContentsObserver overrides.
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;

 private:
  // Kicks off the process of showing a banner for the package designated by
  // |tag_content| on the page at the |expected_url|.
  virtual void HandleMetaTagContent(const std::string& tag_content,
                                    const GURL& expected_url) OVERRIDE;

  JavaObjectWeakGlobalRef weak_java_banner_view_manager_;

  DISALLOW_COPY_AND_ASSIGN(AppBannerManager);
};  // class AppBannerManager

// Register native methods
bool RegisterAppBannerManager(JNIEnv* env);

#endif  // CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_MANAGER_H_
