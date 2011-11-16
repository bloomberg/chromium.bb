// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_FAVICON_TAB_HELPER_H_
#define CHROME_BROWSER_FAVICON_FAVICON_TAB_HELPER_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/callback_old.h"
#include "chrome/browser/favicon/favicon_handler_delegate.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/common/favicon_url.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "googleurl/src/gurl.h"

class FaviconHandler;
class NavigationEntry;
class SkBitmap;
class TabContents;

// FaviconTabHelper works with FaviconHandlers to fetch the favicons.
//
// FetchFavicon fetches the given page's icons. It requests the icons from the
// history backend. If the icon is not available or expired, the icon will be
// downloaded and saved in the history backend.
//
// DownloadImage downloads the specified icon and returns it through the given
// callback.
//
class FaviconTabHelper : public TabContentsObserver,
                         public FaviconHandlerDelegate {
 public:
  explicit FaviconTabHelper(TabContents* tab_contents);
  virtual ~FaviconTabHelper();

  // Initiates loading the favicon for the specified url.
  void FetchFavicon(const GURL& url);

  // Returns the favicon for this tab, or IDR_DEFAULT_FAVICON if the tab does
  // not have a favicon. The default implementation uses the current navigation
  // entry. This will return an isNull bitmap if there are no navigation
  // entries, which should rarely happen.
  SkBitmap GetFavicon() const;

  // Returns true if we are not using the default favicon.
  bool FaviconIsValid() const;

  // Returns whether the favicon should be displayed. If this returns false, no
  // space is provided for the favicon, and the favicon is never displayed.
  virtual bool ShouldDisplayFavicon();

  // Saves the favicon for the current page.
  void SaveFavicon();

  // Initiates loading an image from given |image_url|. Returns a download id
  // for caller to track the request. When download completes, |callback| is
  // called with the three params: the download_id, a boolean flag to indicate
  // whether the download succeeds and a SkBitmap as the downloaded image.
  // Note that |image_size| is a hint for images with multiple sizes. The
  // downloaded image is not resized to the given image_size. If 0 is passed,
  // the first frame of the image is returned.
  typedef Callback3<int, bool, const SkBitmap&>::Type ImageDownloadCallback;
  int DownloadImage(const GURL& image_url,
                    int image_size,
                    history::IconType icon_type,
                    ImageDownloadCallback* callback);

  // Message Handler.  Must be public, because also called from
  // PrerenderContents.
  void OnUpdateFaviconURL(int32 page_id,
                          const std::vector<FaviconURL>& candidates);

  // FaviconHandlerDelegate methods.
  virtual NavigationEntry* GetActiveEntry() OVERRIDE;
  virtual void StartDownload(int id, const GURL& url, int image_size) OVERRIDE;
  virtual void NotifyFaviconUpdated() OVERRIDE;

 private:
  // TabContentsObserver overrides.
  virtual void NavigateToPendingEntry(
      const GURL& url,
      NavigationController::ReloadType reload_type) OVERRIDE;
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const ViewHostMsg_FrameNavigate_Params& params) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnDidDownloadFavicon(int id,
                            const GURL& image_url,
                            bool errored,
                            const SkBitmap& image);

  Profile* profile_;

  scoped_ptr<FaviconHandler> favicon_handler_;

  // Handles downloading touchicons. It is NULL if
  // browser_defaults::kEnableTouchIcon is false.
  scoped_ptr<FaviconHandler> touch_icon_handler_;

  DISALLOW_COPY_AND_ASSIGN(FaviconTabHelper);
};

#endif  // CHROME_BROWSER_FAVICON_FAVICON_TAB_HELPER_H_
