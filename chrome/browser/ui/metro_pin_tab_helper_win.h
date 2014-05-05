// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_METRO_PIN_TAB_HELPER_WIN_H_
#define CHROME_BROWSER_UI_METRO_PIN_TAB_HELPER_WIN_H_

#include <vector>

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/favicon_url.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"

namespace gfx {
class Size;
}

// Per-tab class to help manage metro pinning.
class MetroPinTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<MetroPinTabHelper> {
 public:
  virtual ~MetroPinTabHelper();

  bool IsPinned() const;

  void TogglePinnedToStartScreen();

  // content::WebContentsObserver overrides:
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;
  virtual void DidUpdateFaviconURL(
      const std::vector<content::FaviconURL>& candidates) OVERRIDE;

 private:
  // The FaviconDownloader class handles downloading the favicons when a page
  // is being pinned. After it has downloaded any available favicons it will
  // continue on with the page pinning action.
  class FaviconChooser;

  explicit MetroPinTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<MetroPinTabHelper>;

  // Favicon download callback.
  void DidDownloadFavicon(int id,
                          int http_status_code,
                          const GURL& image_url,
                          const std::vector<SkBitmap>& bitmaps,
                          const std::vector<gfx::Size>& original_bitmap_sizes);

  void UnPinPageFromStartScreen();

  // Called by the |favicon_chooser_| when it has finished.
  void FaviconDownloaderFinished();

  // Candidate Favicon URLs for the current page.
  std::vector<content::FaviconURL> favicon_url_candidates_;

  // The currently active FaviconChooser, if there is one.
  scoped_ptr<FaviconChooser> favicon_chooser_;


  DISALLOW_COPY_AND_ASSIGN(MetroPinTabHelper);
};

#endif  // CHROME_BROWSER_UI_METRO_PIN_TAB_HELPER_WIN_H_
