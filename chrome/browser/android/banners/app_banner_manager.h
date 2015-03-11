// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_MANAGER_H_
#define CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_MANAGER_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/android/banners/app_banner_infobar_delegate.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/browser/ui/android/infobars/app_banner_infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/manifest.h"
#include "third_party/WebKit/public/platform/modules/app_banner/WebAppBannerPromptReply.h"

namespace content {
struct FrameNavigateParams;
struct LoadCommittedDetails;
}  // namespace content

namespace infobars {
class InfoBar;
}  // namspace infobars

/**
 * Manages when an app banner is created or dismissed.
 *
 * Hooks the wiring together for getting the data for a particular app.
 * Monitors at most one app at a time, tracking the info for the most recently
 * requested app. Any work in progress for other apps is discarded.
 *
 * TODO(dfalcantara): Update this when the pipeline requirements resolidify.
 */

namespace banners {

class AppBannerManager : public content::WebContentsObserver {
 public:
  class BannerBitmapFetcher;

  AppBannerManager(JNIEnv* env, jobject obj, int icon_size);
  ~AppBannerManager() override;

  // Destroys the AppBannerManager.
  void Destroy(JNIEnv* env, jobject obj);

  // Observes a new WebContents, if necessary.
  void ReplaceWebContents(JNIEnv* env,
                          jobject obj,
                          jobject jweb_contents);

  // Called when the Java-side has retrieved information for the app.
  // Returns |false| if an icon fetch couldn't be kicked off.
  bool OnAppDetailsRetrieved(JNIEnv* env,
                             jobject obj,
                             jobject japp_data,
                             jstring japp_title,
                             jstring japp_package,
                             jstring jicon_url);

  // Fetches the icon at the given URL asynchronously.
  // Returns |false| if this couldn't be kicked off.
  bool FetchIcon(const GURL& image_url);

  // Called when everything required to show a banner is ready.
  void OnFetchComplete(BannerBitmapFetcher* fetcher,
                       const GURL url,
                       const SkBitmap* icon);

  // Return whether a BitmapFetcher is active.
  bool IsFetcherActive(JNIEnv* env, jobject jobj);

  // Returns the current time.
  static base::Time GetCurrentTime();

  // WebContentsObserver overrides.
  void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  bool OnMessageReceived(const IPC::Message& message) override;
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override;

 private:
  friend class AppBannerManagerTest;

  // Returns whether the given Manifest is following the requirements to show
  // a web app banner.
  static bool IsManifestValid(const content::Manifest& manifest);

  // Called when the manifest has been retrieved, or if there is no manifest to
  // retrieve.
  void OnDidGetManifest(const content::Manifest& manifest);

  // Called when the renderer has returned information about the meta tag.
  // If there is some metadata for the play store tag, this kicks off the
  // process of showing a banner for the package designated by |tag_content| on
  // the page at the |expected_url|.
  void OnDidRetrieveMetaTagContent(bool success,
                                   const std::string& tag_name,
                                   const std::string& tag_content,
                                   const GURL& expected_url);

  // Called when the result of the CheckHasServiceWorker query has completed.
  void OnDidCheckHasServiceWorker(bool has_service_worker);

  // Record that the banner could be shown at this point, if the triggering
  // heuristic allowed.
  void RecordCouldShowBanner(const std::string& package_or_start_url);

  // Check if the banner should be shown.
  bool CheckIfShouldShow(const std::string& package_or_start_url);

  // Record that the banner was shown.
  void RecordDidShowBanner(const std::string& package_or_start_url);

  // Cancels an active BitmapFetcher, stopping its banner from appearing.
  void CancelActiveFetcher();

  // Whether or not the banners should appear for native apps.
  static bool IsEnabledForNativeApps();

  // Called after the manager sends a message to the renderer regarding its
  // intention to show a prompt. The renderer will send a message back with the
  // opportunity to cancel.
  void OnBannerPromptReply(content::RenderFrameHost* render_frame_host,
                           int request_id,
                           blink::WebAppBannerPromptReply reply);

  // Icon size that we want to be use for the launcher.
  const int preferred_icon_size_;

  // Fetches the icon for an app.  Weakly held because they delete themselves.
  BannerBitmapFetcher* fetcher_;

  GURL validated_url_;
  GURL app_icon_url_;

  base::string16 app_title_;

  content::Manifest web_app_data_;

  base::android::ScopedJavaGlobalRef<jobject> native_app_data_;
  std::string native_app_package_;

  scoped_ptr<SkBitmap> icon_;

  // AppBannerManager on the Java side.
  JavaObjectWeakGlobalRef weak_java_banner_view_manager_;

  // A weak pointer is used as the lifetime of the ServiceWorkerContext is
  // longer than the lifetime of this banner manager. The banner manager
  // might be gone when calls sent to the ServiceWorkerContext are completed.
  base::WeakPtrFactory<AppBannerManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppBannerManager);
};  // class AppBannerManager

// Register native methods
bool RegisterAppBannerManager(JNIEnv* env);

}  // namespace banners

#endif  // CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_MANAGER_H_
