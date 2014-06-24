// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_FAVICON_TAB_HELPER_H_
#define CHROME_BROWSER_FAVICON_FAVICON_TAB_HELPER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
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
  virtual ~FaviconTabHelper();

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
  virtual void DidUpdateFaviconURL(
      const std::vector<content::FaviconURL>& candidates) OVERRIDE;

  // Saves the favicon for the current page.
  void SaveFavicon();

  // FaviconDriver methods.
  virtual int StartDownload(const GURL& url, int max_bitmap_size) OVERRIDE;
  virtual void NotifyFaviconUpdated(bool icon_url_changed) OVERRIDE;
  virtual bool IsOffTheRecord() OVERRIDE;
  virtual const gfx::Image GetActiveFaviconImage() OVERRIDE;
  virtual const GURL GetActiveFaviconURL() OVERRIDE;
  virtual bool GetActiveFaviconValidity() OVERRIDE;
  virtual const GURL GetActiveURL() OVERRIDE;
  virtual void SetActiveFaviconImage(gfx::Image image) OVERRIDE;
  virtual void SetActiveFaviconURL(GURL url) OVERRIDE;
  virtual void SetActiveFaviconValidity(bool validity) OVERRIDE;

  // Favicon download callback.
  void DidDownloadFavicon(
      int id,
      int http_status_code,
      const GURL& image_url,
      const std::vector<SkBitmap>& bitmaps,
      const std::vector<gfx::Size>& original_bitmap_sizes);

 private:
  explicit FaviconTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<FaviconTabHelper>;

  // content::WebContentsObserver overrides.
  virtual void DidStartNavigationToPendingEntry(
      const GURL& url,
      content::NavigationController::ReloadType reload_type) OVERRIDE;
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;

  // Helper method that returns the active navigation entry's favicon.
  content::FaviconStatus& GetFaviconStatus();

  Profile* profile_;

  FaviconClient* client_;

  std::vector<content::FaviconURL> favicon_urls_;

  scoped_ptr<FaviconHandler> favicon_handler_;

  // Handles downloading touchicons. It is NULL if
  // browser_defaults::kEnableTouchIcon is false.
  scoped_ptr<FaviconHandler> touch_icon_handler_;

  DISALLOW_COPY_AND_ASSIGN(FaviconTabHelper);
};

#endif  // CHROME_BROWSER_FAVICON_FAVICON_TAB_HELPER_H_
