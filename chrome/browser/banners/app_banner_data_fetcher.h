// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BANNERS_APP_BANNER_DATA_FETCHER_H_
#define CHROME_BROWSER_BANNERS_APP_BANNER_DATA_FETCHER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/manifest.h"
#include "third_party/WebKit/public/platform/modules/app_banner/WebAppBannerPromptReply.h"

namespace chrome {
class BitmapFetcher;
}  // namespace chrome

namespace infobars {
class InfoBar;
}  // namespace infobars

namespace banners {
class AppBannerDataFetcher;

// Fetches data required to show a web app banner for the URL currently shown by
// the WebContents.
class AppBannerDataFetcher
    : public base::RefCounted<AppBannerDataFetcher>,
      public chrome::BitmapFetcherDelegate,
      public content::WebContentsObserver {
 public:
  class Observer {
   public:
    virtual void OnDecidedWhetherToShow(AppBannerDataFetcher* fetcher,
                                        bool will_show) = 0;
    virtual void OnFetcherDestroyed(AppBannerDataFetcher* fetcher) = 0;
  };

  class Delegate {
   public:
    // Called when no valid manifest was found.  Returns |true| if the fetcher
    // needs to remain active and wait for a callback.
    virtual bool OnInvalidManifest(AppBannerDataFetcher* fetcher) = 0;
  };

  // Returns the current time.
  static base::Time GetCurrentTime();

  // Fast-forwards the current time for testing.
  static void SetTimeDeltaForTesting(int days);

  AppBannerDataFetcher(content::WebContents* web_contents,
                       base::WeakPtr<Delegate> weak_delegate,
                       int ideal_icon_size);

  // Begins creating a banner for the URL being displayed by the Delegate's
  // WebContents.
  void Start(const GURL& validated_url);

  // Stops the pipeline when any asynchronous calls return.
  void Cancel();

  // Replaces the WebContents that is being observed.
  void ReplaceWebContents(content::WebContents* web_contents);

  // Adds an observer to the list.
  void AddObserverForTesting(Observer* observer);

  // Removes an observer from the list.
  void RemoveObserverForTesting(Observer* observer);

  // Returns whether or not the pipeline has been stopped.
  bool is_active() { return is_active_; }

  // Returns the URL that kicked off the banner data retrieval.
  const GURL& validated_url() { return validated_url_; }

  // WebContentsObserver overrides.
  void WebContentsDestroyed() override;
  void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override;
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override;

 protected:
  ~AppBannerDataFetcher() override;

  // Return a string describing what type of banner is being created. Used when
  // alerting websites that a banner is about to be created.
  virtual std::string GetBannerType();

  // Called after the manager sends a message to the renderer regarding its
  // intention to show a prompt. The renderer will send a message back with the
  // opportunity to cancel.
  void OnBannerPromptReply(content::RenderFrameHost* render_frame_host,
                           int request_id,
                           blink::WebAppBannerPromptReply reply);

  content::WebContents* GetWebContents();
  virtual std::string GetAppIdentifier();
  const content::Manifest& web_app_data() { return web_app_data_; }
  void set_app_title(const base::string16& title) { app_title_ = title; }

  // Fetches the icon at the given URL asynchronously, returning |false| if a
  // load could not be started.
  bool FetchIcon(const GURL& image_url);

  // Creates a banner for the app using the given |icon|.
  virtual infobars::InfoBar* CreateBanner(const SkBitmap* icon,
                                          const base::string16& title);

  // Records that a banner was shown. The |event_name| corresponds to the RAPPOR
  // metric being recorded.
  void RecordDidShowBanner(const std::string& event_name);

 private:
  // Callbacks for data retrieval.
  void OnDidGetManifest(const content::Manifest& manifest);
  void OnDidCheckHasServiceWorker(bool has_service_worker);
  void OnFetchComplete(const GURL url, const SkBitmap* icon) override;

  // Shows a banner for the app, if the given |icon| is valid.
  virtual void ShowBanner(const SkBitmap* icon);

  // Record that the banner could be shown at this point, if the triggering
  // heuristic allowed.
  void RecordCouldShowBanner();

  // Returns whether the banner should be shown.
  bool CheckIfShouldShowBanner();

  // Returns whether the given Manifest is following the requirements to show
  // a web app banner.
  static bool IsManifestValid(const content::Manifest& manifest);

  const int ideal_icon_size_;
  const base::WeakPtr<Delegate> weak_delegate_;
  ObserverList<Observer> observer_list_;
  bool is_active_;
  scoped_ptr<chrome::BitmapFetcher> bitmap_fetcher_;
  scoped_ptr<SkBitmap> app_icon_;

  GURL validated_url_;
  base::string16 app_title_;
  content::Manifest web_app_data_;

  friend class AppBannerDataFetcherUnitTest;
  friend class base::RefCounted<AppBannerDataFetcher>;
  DISALLOW_COPY_AND_ASSIGN(AppBannerDataFetcher);
};

}  // namespace banners

#endif  // CHROME_BROWSER_BANNERS_APP_BANNER_DATA_FETCHER_H_
