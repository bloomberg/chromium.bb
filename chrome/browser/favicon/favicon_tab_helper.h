// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_FAVICON_TAB_HELPER_H_
#define CHROME_BROWSER_FAVICON_FAVICON_TAB_HELPER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/observer_list.h"
#include "components/favicon/core/browser/favicon_client.h"
#include "components/favicon/core/favicon_driver.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/favicon_url.h"

namespace gfx {
class Image;
}

namespace content {
struct FaviconStatus;
}

class GURL;
class FaviconHandler;
class FaviconTabHelperObserver;
class Profile;
class SkBitmap;

// FaviconTabHelper works with FaviconHandlers to fetch the favicons.
//
// FetchFavicon fetches the given page's icons. It requests the icons from the
// history backend. If the icon is not available or expired, the icon will be
// downloaded and saved in the history backend.
//
class FaviconTabHelper : public content::WebContentsObserver,
                         public FaviconDriver,
                         public content::WebContentsUserData<FaviconTabHelper> {
 public:
  ~FaviconTabHelper() override;

  // Initiates loading the favicon for the specified url.
  void FetchFavicon(const GURL& url);

  // Returns the favicon for this tab, or IDR_DEFAULT_FAVICON if the tab does
  // not have a favicon. The default implementation uses the current navigation
  // entry. This will return an empty bitmap if there are no navigation
  // entries, which should rarely happen.
  gfx::Image GetFavicon() const;

  // Returns true if we have the favicon for the page.
  bool FaviconIsValid() const;

  // Returns whether the favicon should be displayed. If this returns false, no
  // space is provided for the favicon, and the favicon is never displayed.
  virtual bool ShouldDisplayFavicon();

  // Returns the current tab's favicon urls. If this is empty,
  // DidUpdateFaviconURL has not yet been called for the current navigation.
  const std::vector<content::FaviconURL>& favicon_urls() const {
    return favicon_urls_;
  }

  // content::WebContentsObserver override. Must be public, because also
  // called from PrerenderContents.
  void DidUpdateFaviconURL(
      const std::vector<content::FaviconURL>& candidates) override;

  // Saves the favicon for the current page.
  void SaveFavicon();

  void AddObserver(FaviconTabHelperObserver* observer);
  void RemoveObserver(FaviconTabHelperObserver* observer);

  // FaviconDriver methods.
  int StartDownload(const GURL& url, int max_bitmap_size) override;
  bool IsOffTheRecord() override;
  const gfx::Image GetActiveFaviconImage() override;
  const GURL GetActiveFaviconURL() override;
  bool GetActiveFaviconValidity() override;
  const GURL GetActiveURL() override;
  void OnFaviconAvailable(const gfx::Image& image,
                          const GURL& url,
                          bool is_active_favicon) override;

  // Favicon download callback.
  void DidDownloadFavicon(
      int id,
      int http_status_code,
      const GURL& image_url,
      const std::vector<SkBitmap>& bitmaps,
      const std::vector<gfx::Size>& original_bitmap_sizes);

 private:
  friend class content::WebContentsUserData<FaviconTabHelper>;
  friend class FaviconTabHelperTest;

  explicit FaviconTabHelper(content::WebContents* web_contents);

  // content::WebContentsObserver overrides.
  void DidStartNavigationToPendingEntry(
      const GURL& url,
      content::NavigationController::ReloadType reload_type) override;
  void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override;

  // Sets whether the page's favicon is valid (if false, the default favicon is
  // being used). Requires GetActiveURL() to be valid.
  void SetActiveFaviconValidity(bool validity);

  // Sets the URL of the favicon's bitmap.
  void SetActiveFaviconURL(GURL url);

  // Sets the bitmap of the current page's favicon.
  void SetActiveFaviconImage(gfx::Image image);

  // Helper method that returns the active navigation entry's favicon.
  content::FaviconStatus& GetFaviconStatus();

  Profile* profile_;

  FaviconClient* client_;

  std::vector<content::FaviconURL> favicon_urls_;

  // Bypass cache when downloading favicons for this page URL.
  GURL bypass_cache_page_url_;

  scoped_ptr<FaviconHandler> favicon_handler_;

  // Handles downloading touchicons. It is NULL if
  // browser_defaults::kEnableTouchIcon is false.
  scoped_ptr<FaviconHandler> touch_icon_handler_;

  ObserverList<FaviconTabHelperObserver> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(FaviconTabHelper);
};

#endif  // CHROME_BROWSER_FAVICON_FAVICON_TAB_HELPER_H_
