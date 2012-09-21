// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_FAVICON_LOADER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_FAVICON_LOADER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "chrome/common/favicon_url.h"
#include "content/public/browser/web_contents_observer.h"

class GURL;
class SkBitmap;

namespace internal {
class FaviconBitmapHandler;
}

// LauncherFaviconLoader handles updates to the list of favicon urls and
// retrieves the appropriately sized favicon for panels in the Launcher.

class LauncherFaviconLoader : public content::WebContentsObserver {
 public:
  class Delegate {
   public:
    virtual void FaviconUpdated() = 0;

   protected:
    virtual ~Delegate() {}
  };

  LauncherFaviconLoader(Delegate* delegate,
                        content::WebContents* web_contents);
  virtual ~LauncherFaviconLoader();

  // content::WebContentsObserver overrides.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  content::WebContents* web_contents() {
    return content::WebContentsObserver::web_contents();
  }

  // Returns an appropriately sized favicon for the Launcher. If none are
  // available will return an isNull bitmap.
  SkBitmap GetFavicon() const;

  // Returns true if the loader is waiting for downloads to finish.
  bool HasPendingDownloads() const;

 private:
  // Message Handlers.
  void OnUpdateFaviconURL(int32 page_id,
                          const std::vector<FaviconURL>& candidates);

  void OnDidDownloadFavicon(int id,
                            const GURL& image_url,
                            bool errored,
                            int requested_size,
                            const std::vector<SkBitmap>& bitmaps);

  scoped_ptr<internal::FaviconBitmapHandler> favicon_handler_;

  DISALLOW_COPY_AND_ASSIGN(LauncherFaviconLoader);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_FAVICON_LOADER_H_
