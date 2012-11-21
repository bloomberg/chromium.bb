// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_METRO_PIN_TAB_HELPER_WIN_H_
#define CHROME_BROWSER_UI_METRO_PIN_TAB_HELPER_WIN_H_

#include <vector>

#include "chrome/browser/favicon/favicon_download_helper.h"
#include "chrome/browser/favicon/favicon_download_helper_delegate.h"
#include "chrome/common/favicon_url.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"

// Per-tab class to help manage metro pinning.
class MetroPinTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<MetroPinTabHelper>,
      public FaviconDownloadHelperDelegate {
 public:
  virtual ~MetroPinTabHelper();

  bool IsPinned() const;

  void TogglePinnedToStartScreen();

  // content::WebContentsObserver overrides:
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;

 private:
  // The FaviconDownloader class handles downloading the favicons when a page
  // is being pinned. After it has downloaded any available favicons it will
  // continue on with the page pinning action.
  class FaviconChooser;

  explicit MetroPinTabHelper(content::WebContents* tab_contents);
  friend class content::WebContentsUserData<MetroPinTabHelper>;

  // FaviconDownloadHelperDelegate overrides.
  void OnUpdateFaviconURL(int32 page_id,
                          const std::vector<FaviconURL>& candidates) OVERRIDE;

  void OnDidDownloadFavicon(int id,
                            const GURL& image_url,
                            bool errored,
                            int requested_size,
                            const std::vector<SkBitmap>& bitmaps) OVERRIDE;

  void UnPinPageFromStartScreen();

  // Called by the |favicon_downloader_| when it has finished.
  void FaviconDownloaderFinished();

  // Candidate Favicon URLs for the current page.
  std::vector<FaviconURL> favicon_url_candidates_;

  // The currently active FaviconChooser, if there is one.
  scoped_ptr<FaviconChooser> favicon_chooser_;

  // FaviconDownloadHelper handles the sending and listening for IPC messages.
  FaviconDownloadHelper favicon_download_helper_;

  DISALLOW_COPY_AND_ASSIGN(MetroPinTabHelper);
};

#endif  // CHROME_BROWSER_UI_METRO_PIN_TAB_HELPER_WIN_H_
