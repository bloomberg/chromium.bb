// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_DATA_RETRIEVER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_DATA_RETRIEVER_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/common/web_application_info.h"

namespace content {
class WebContents;
}

namespace extensions {

// Class used by BookmarkAppInstallationTask to retrieve the necessary
// information to install an app. Should only be called from the UI thread.
class BookmarkAppDataRetriever {
 public:
  using GetWebApplicationInfoCallback =
      base::OnceCallback<void(std::unique_ptr<WebApplicationInfo>)>;
  using GetIconsCallback =
      base::OnceCallback<void(std::vector<WebApplicationInfo::IconInfo>)>;

  BookmarkAppDataRetriever();
  virtual ~BookmarkAppDataRetriever();

  // Runs |callback| with the result of retrieving the WebApplicationInfo from
  // |web_contents|.
  virtual void GetWebApplicationInfo(content::WebContents* web_contents,
                                     GetWebApplicationInfoCallback callback);

  // Downloads icons from |icon_urls|. If icons are missing for certain required
  // sizes, generates them based on |app_url|. Runs |callback| with a vector of
  // the retrieved and generated icons.
  virtual void GetIcons(const GURL& app_url,
                        const std::vector<GURL>& icon_urls,
                        GetIconsCallback callback);

 private:
  void OnGetWebApplicationInfo(
      chrome::mojom::ChromeRenderFrameAssociatedPtr chrome_render_frame,
      content::WebContents* web_contents,
      int last_committed_nav_entry_unique_id,
      const WebApplicationInfo& web_app_info);
  void OnGetWebApplicationInfoFailed();

  // Saved callback from GetWebApplicationInfo().
  GetWebApplicationInfoCallback get_web_app_info_callback_;

  base::WeakPtrFactory<BookmarkAppDataRetriever> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BookmarkAppDataRetriever);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_DATA_RETRIEVER_H_
