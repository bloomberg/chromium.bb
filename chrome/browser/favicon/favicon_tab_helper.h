// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_FAVICON_TAB_HELPER_H_
#define CHROME_BROWSER_FAVICON_FAVICON_TAB_HELPER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "chrome/browser/favicon/favicon_handler_delegate.h"
#include "chrome/browser/history/history_types.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/favicon_url.h"

namespace gfx {
class Image;
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
                         public FaviconHandlerDelegate,
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

  // Allows the client to determine if they want to fetch the Favicons as
  // they are discovered.
  void set_should_fetch_icons(bool fetch) {
    should_fetch_icons_ = fetch;
  }

  // content::WebContentsObserver override. Must be public, because also
  // called from PrerenderContents.
  virtual void DidUpdateFaviconURL(
      int32 page_id,
      const std::vector<content::FaviconURL>& candidates) OVERRIDE;

  // Saves the favicon for the current page.
  void SaveFavicon();

  // FaviconHandlerDelegate methods.
  virtual content::NavigationEntry* GetActiveEntry() OVERRIDE;
  virtual int StartDownload(const GURL& url, int image_size) OVERRIDE;
  virtual void NotifyFaviconUpdated() OVERRIDE;

 private:
  explicit FaviconTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<FaviconTabHelper>;

  // content::WebContentsObserver overrides.
  virtual void NavigateToPendingEntry(
      const GURL& url,
      content::NavigationController::ReloadType reload_type) OVERRIDE;
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;

  // Favicon download callback.
  void DidDownloadFavicon(
      int id,
      const GURL& image_url,
      int requested_size,
      const std::vector<SkBitmap>& bitmaps);

  Profile* profile_;
  bool should_fetch_icons_;

  scoped_ptr<FaviconHandler> favicon_handler_;

  // Handles downloading touchicons. It is NULL if
  // browser_defaults::kEnableTouchIcon is false.
  scoped_ptr<FaviconHandler> touch_icon_handler_;

  DISALLOW_COPY_AND_ASSIGN(FaviconTabHelper);
};

#endif  // CHROME_BROWSER_FAVICON_FAVICON_TAB_HELPER_H_
