// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_DATA_RETRIEVER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_DATA_RETRIEVER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/common/chrome_render_frame.mojom.h"

namespace content {
class WebContents;
}

struct WebApplicationInfo;

namespace extensions {

// Class used by BookmarkAppInstallationTask to retrieve the necessary
// information to install an app. Should only be called from the UI thread.
class BookmarkAppDataRetriever {
 public:
  using GetWebApplicationInfoCallback =
      base::OnceCallback<void(base::Optional<WebApplicationInfo>)>;

  BookmarkAppDataRetriever();
  virtual ~BookmarkAppDataRetriever();

  // Runs |callback| with the result of retrieving the WebApplicationInfo from
  // |web_contents|.
  virtual void GetWebApplicationInfo(content::WebContents* web_contents,
                                     GetWebApplicationInfoCallback callback);

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
