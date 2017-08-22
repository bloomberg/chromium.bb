// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPPS_ADD_TO_HOMESCREEN_DATA_FETCHER_H_
#define CHROME_BROWSER_ANDROID_WEBAPPS_ADD_TO_HOMESCREEN_DATA_FETCHER_H_

#include <utility>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/timer/timer.h"
#include "chrome/browser/android/shortcut_info.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace favicon_base {
struct FaviconRawBitmapResult;
}

class InstallableManager;
struct InstallableData;
struct WebApplicationInfo;

// Aysnchronously fetches and processes data needed to create a shortcut for an
// Android Home screen launcher.
class AddToHomescreenDataFetcher : public content::WebContentsObserver {
 public:
  class Observer {
   public:
    // Called when the installable check is complete.
    virtual void OnDidDetermineWebApkCompatibility(
        bool is_webapk_compatible) = 0;

    // Called when the homescreen icon title (and possibly information from the
    // web manifest) is available. Will be called after
    // OnDidDetermineWebApkCompatibility.
    virtual void OnUserTitleAvailable(const base::string16& title,
                                      const GURL& url) = 0;

    // Called when all the data needed to create a shortcut is available.
    virtual void OnDataAvailable(const ShortcutInfo& info,
                                 const SkBitmap& primary_icon,
                                 const SkBitmap& badge_icon) = 0;

   protected:
    virtual ~Observer() {}
  };

  // Initialize the fetcher by requesting the information about the page from
  // the renderer process. The initialization is asynchronous and
  // OnDidGetWebApplicationInfo is expected to be called when finished.
  // |observer| must outlive AddToHomescreenDataFetcher.
  AddToHomescreenDataFetcher(content::WebContents* web_contents,
                             int ideal_icon_size_in_px,
                             int minimum_icon_size_in_px,
                             int ideal_splash_image_size_in_px,
                             int minimum_splash_image_size_in_px,
                             int badge_size_in_px,
                             int data_timeout_ms,
                             bool check_webapk_compatible,
                             Observer* observer);

  ~AddToHomescreenDataFetcher() override;

  // IPC message received when the initialization is finished.
  void OnDidGetWebApplicationInfo(const WebApplicationInfo& web_app_info);

  // Accessors, etc.
  const SkBitmap& badge_icon() const { return badge_icon_; }
  const SkBitmap& primary_icon() const { return primary_icon_; }
  ShortcutInfo& shortcut_info() { return shortcut_info_; }

 private:
  // WebContentsObserver:
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* sender) override;

  // Called if either InstallableManager or the favicon fetch takes too long.
  void OnDataTimedout();

  // Called when InstallableManager finishes looking for a manifest and icon.
  void OnDidGetManifestAndIcons(const InstallableData& data);

  // Called when InstallableManager finishes checking for installability.
  void OnDidPerformInstallableCheck(const InstallableData& data);

  // Grabs the favicon for the current URL.
  void FetchFavicon();
  void OnFaviconFetched(
      const favicon_base::FaviconRawBitmapResult& bitmap_result);

  // Creates the primary launcher icon from the given |icon|.
  void CreateLauncherIcon(const SkBitmap& icon);

  // Notifies the observer that the shortcut data is all available.
  void NotifyObserver(const std::pair<SkBitmap, bool /*is_generated*/>& icon);

  InstallableManager* installable_manager_;
  Observer* observer_;

  // The icons must only be set on the UI thread for thread safety.
  SkBitmap raw_primary_icon_;
  SkBitmap badge_icon_;
  SkBitmap primary_icon_;
  ShortcutInfo shortcut_info_;

  base::CancelableTaskTracker favicon_task_tracker_;
  base::OneShotTimer data_timeout_timer_;

  const int ideal_icon_size_in_px_;
  const int minimum_icon_size_in_px_;
  const int ideal_splash_image_size_in_px_;
  const int minimum_splash_image_size_in_px_;
  const int badge_size_in_px_;
  const int data_timeout_ms_;

  // Indicates whether to check WebAPK compatibility.
  bool check_webapk_compatibility_;
  bool is_waiting_for_web_application_info_;

  base::WeakPtrFactory<AddToHomescreenDataFetcher> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AddToHomescreenDataFetcher);
};

#endif  // CHROME_BROWSER_ANDROID_WEBAPPS_ADD_TO_HOMESCREEN_DATA_FETCHER_H_
