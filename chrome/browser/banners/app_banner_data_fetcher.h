// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BANNERS_APP_BANNER_DATA_FETCHER_H_
#define CHROME_BROWSER_BANNERS_APP_BANNER_DATA_FETCHER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/common/web_application_info.h"
#include "content/public/browser/browser_thread.h"
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
class AppBannerDataFetcher : public base::RefCountedThreadSafe<
                                 AppBannerDataFetcher,
                                 content::BrowserThread::DeleteOnUIThread>,
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
    // Called to handle a non-web app. Returns |true| if the non-web app can be
    // handled, and the fetcher needs to remain active and wait for a callback.
    virtual bool HandleNonWebApp(const std::string& platform,
                                 const GURL& url,
                                 const std::string& id,
                                 bool is_debug_mode) = 0;
  };

  // Returns the current time.
  static base::Time GetCurrentTime();

  // Fast-forwards the current time for testing.
  static void SetTimeDeltaForTesting(int days);

  AppBannerDataFetcher(content::WebContents* web_contents,
                       base::WeakPtr<Delegate> weak_delegate,
                       int ideal_icon_size_in_dp,
                       int minimum_icon_size_in_dp,
                       bool is_debug_mode);

  // Begins creating a banner for the URL being displayed by the Delegate's
  // WebContents.
  void Start(const GURL& validated_url, ui::PageTransition transition_type);

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

  // Returns whether the beforeinstallprompt Javascript event was canceled.
  bool was_canceled_by_page() { return was_canceled_by_page_; }

  // Returns whether the page has validly requested that the banner be shown
  // by calling prompt() on the beforeinstallprompt Javascript event.
  bool page_requested_prompt() { return page_requested_prompt_; }

  // Returns true when it was created by the user action in DevTools or
  // "bypass-app-banner-engagement-checks" flag is set.
  bool is_debug_mode() const { return is_debug_mode_; }

  // Returns the type of transition which triggered this fetch.
  ui::PageTransition transition_type() { return transition_type_; }

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
                           blink::WebAppBannerPromptReply reply,
                           std::string referrer);

  // Called when the client has prevented a banner from being shown, and is
  // now requesting that it be shown later.
  void OnRequestShowAppBanner(content::RenderFrameHost* render_frame_host,
                              int request_id);

  content::WebContents* GetWebContents();
  virtual std::string GetAppIdentifier();
  const content::Manifest& web_app_data() { return web_app_data_; }
  void set_app_title(const base::string16& title) { app_title_ = title; }
  int event_request_id() { return event_request_id_; }

  // Fetches the icon at the given URL asynchronously, returning |false| if a
  // load could not be started.
  bool FetchAppIcon(content::WebContents* web_contents, const GURL& url);

  // Records that a banner was shown. The |event_name| corresponds to the RAPPOR
  // metric being recorded.
  void RecordDidShowBanner(const std::string& event_name);

 private:
  // Callbacks for data retrieval.
  void OnDidHasManifest(bool has_manifest);
  void OnDidGetManifest(const content::Manifest& manifest);
  void OnDidCheckHasServiceWorker(bool has_service_worker);
  void OnAppIconFetched(const SkBitmap& bitmap);

  // Called when it is determined that the webapp has fulfilled the initial
  // criteria of having a manifest and a service worker.
  void OnHasServiceWorker(content::WebContents* web_contents);

  // Returns whether the given web app has already been installed.
  // Implemented on desktop platforms only.
  virtual bool IsWebAppInstalled(content::BrowserContext* browser_context,
                                 const GURL& start_url);

  // Record that the banner could be shown at this point, if the triggering
  // heuristic allowed.
  void RecordCouldShowBanner();

  // Creates a banner for the app using the given |icon|.
  virtual void ShowBanner(const SkBitmap* icon,
                          const base::string16& title,
                          const std::string& referrer) = 0;

  // Returns whether the banner should be shown.
  bool CheckIfShouldShowBanner();

  // Returns whether the fetcher is active and web contents have not been
  // closed.
  bool CheckFetcherIsStillAlive(content::WebContents* web_contents);

  // Returns whether the given Manifest is following the requirements to show
  // a web app banner.
  static bool IsManifestValidForWebApp(const content::Manifest& manifest,
                                       content::WebContents* web_contents,
                                       bool is_debug_mode);

  const base::WeakPtr<Delegate> weak_delegate_;
  const int ideal_icon_size_in_dp_;
  const int minimum_icon_size_in_dp_;
  base::ObserverList<Observer> observer_list_;
  bool is_active_;
  bool was_canceled_by_page_;
  bool page_requested_prompt_;
  const bool is_debug_mode_;
  ui::PageTransition transition_type_;
  int event_request_id_;
  scoped_ptr<SkBitmap> app_icon_;
  std::string referrer_;

  GURL validated_url_;
  base::string16 app_title_;
  content::Manifest web_app_data_;

  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<AppBannerDataFetcher>;
  friend class AppBannerDataFetcherUnitTest;
  friend class base::RefCounted<AppBannerDataFetcher>;
  DISALLOW_COPY_AND_ASSIGN(AppBannerDataFetcher);
};

}  // namespace banners

#endif  // CHROME_BROWSER_BANNERS_APP_BANNER_DATA_FETCHER_H_
