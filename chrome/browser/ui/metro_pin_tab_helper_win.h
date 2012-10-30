// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_METRO_PIN_TAB_HELPER_WIN_H_
#define CHROME_BROWSER_UI_METRO_PIN_TAB_HELPER_WIN_H_

#include <vector>

#include "chrome/common/favicon_url.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"

// Per-tab class to help manage metro pinning.
class MetroPinTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<MetroPinTabHelper> {
 public:
  virtual ~MetroPinTabHelper();

  bool is_pinned() const { return is_pinned_; }

  void TogglePinnedToStartScreen();

  // content::WebContentsObserver overrides:
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  // The FaviconDownloader class handles downloading the favicons when a page
  // is being pinned. After it has downloaded any available favicons it will
  // continue on with the page pinning action.
  class FaviconDownloader;

  explicit MetroPinTabHelper(content::WebContents* tab_contents);
  friend class content::WebContentsUserData<MetroPinTabHelper>;

  // Message Handlers.
  void OnUpdateFaviconURL(int32 page_id,
                          const std::vector<FaviconURL>& candidates);
  void OnDidDownloadFavicon(int id,
                            const GURL& image_url,
                            bool errored,
                            int requested_size,
                            const std::vector<SkBitmap>& bitmaps);

  // Queries the metro driver about the pinned state of the current URL.
  void UpdatePinnedStateForCurrentURL();

  void UnPinPageFromStartScreen();

  // Called by the |favicon_downloader_| when it has finished.
  void FaviconDownloaderFinished();

  // Whether the current URL is pinned to the metro start screen.
  bool is_pinned_;

  // Candidate Favicon URLs for the current page.
  std::vector<FaviconURL> favicon_url_candidates_;

  // The currently active FaviconDownloader, if there is one.
  scoped_ptr<FaviconDownloader> favicon_downloader_;

  DISALLOW_COPY_AND_ASSIGN(MetroPinTabHelper);
};

#endif  // CHROME_BROWSER_UI_METRO_PIN_TAB_HELPER_WIN_H_
