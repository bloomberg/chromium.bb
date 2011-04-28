// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_FAVICON_TAB_HELPER_H_
#define CHROME_BROWSER_FAVICON_FAVICON_TAB_HELPER_H_
#pragma once

#include "base/basictypes.h"
#include "base/callback.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/common/favicon_url.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "googleurl/src/gurl.h"

class FaviconHandler;
class NavigationEntry;
class Profile;
class RefCountedMemory;
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
class FaviconTabHelper : public TabContentsObserver {
 public:
  explicit FaviconTabHelper(TabContents* tab_contents);
  virtual ~FaviconTabHelper();

  // Initiates loading the favicon for the specified url.
  void FetchFavicon(const GURL& url);

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

 private:
  // TabContentsObserver overrides.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnDidDownloadFavicon(int id,
                            const GURL& image_url,
                            bool errored,
                            const SkBitmap& image);

  scoped_ptr<FaviconHandler> favicon_handler_;

  // Handles downloading touchicons. It is NULL if
  // browser_defaults::kEnableTouchIcon is false.
  scoped_ptr<FaviconHandler> touch_icon_handler_;

  DISALLOW_COPY_AND_ASSIGN(FaviconTabHelper);
};

#endif  // CHROME_BROWSER_FAVICON_FAVICON_TAB_HELPER_H_
