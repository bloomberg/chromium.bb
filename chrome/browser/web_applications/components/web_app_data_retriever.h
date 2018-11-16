// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_DATA_RETRIEVER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_DATA_RETRIEVER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/chrome_render_frame.mojom.h"

struct InstallableData;
struct WebApplicationInfo;

namespace blink {
struct Manifest;
}

namespace content {
class WebContents;
}

namespace web_app {

// Class used by BookmarkAppInstallationTask to retrieve the necessary
// information to install an app. Should only be called from the UI thread.
class WebAppDataRetriever {
 public:
  // Returns nullptr for WebApplicationInfo if error.
  using GetWebApplicationInfoCallback =
      base::OnceCallback<void(std::unique_ptr<WebApplicationInfo>)>;
  // |is_installable| is false if installability check failed.
  using CheckInstallabilityCallback =
      base::OnceCallback<void(const blink::Manifest&, bool is_installable)>;
  // Returns empty vector if error.
  using GetIconsCallback =
      base::OnceCallback<void(std::vector<WebApplicationInfo::IconInfo>)>;

  WebAppDataRetriever();
  virtual ~WebAppDataRetriever();

  // Runs |callback| with the result of retrieving the WebApplicationInfo from
  // |web_contents|.
  virtual void GetWebApplicationInfo(content::WebContents* web_contents,
                                     GetWebApplicationInfoCallback callback);

  // Performs installability check and invokes |callback| with manifest.
  virtual void CheckInstallabilityAndRetrieveManifest(
      content::WebContents* web_contents,
      CheckInstallabilityCallback callback);

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

  void OnDidPerformInstallableCheck(CheckInstallabilityCallback callback,
                                    const InstallableData& data);

  // Saved callback from GetWebApplicationInfo().
  GetWebApplicationInfoCallback get_web_app_info_callback_;

  base::WeakPtrFactory<WebAppDataRetriever> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppDataRetriever);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_DATA_RETRIEVER_H_
