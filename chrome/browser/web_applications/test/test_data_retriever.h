// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_DATA_RETRIEVER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_DATA_RETRIEVER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/web_applications/components/web_app_data_retriever.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"

struct WebApplicationInfo;

namespace web_app {

// All WebAppDataRetriever operations are async, so this class posts tasks
// when running callbacks to simulate async behavior in tests as well.
class TestDataRetriever : public WebAppDataRetriever {
 public:
  TestDataRetriever();
  ~TestDataRetriever() override;

  // WebAppDataRetriever:
  void GetWebApplicationInfo(content::WebContents* web_contents,
                             GetWebApplicationInfoCallback callback) override;
  void CheckInstallabilityAndRetrieveManifest(
      content::WebContents* web_contents,
      CheckInstallabilityCallback callback) override;
  void GetIcons(content::WebContents* web_contents,
                const std::vector<GURL>& icon_urls,
                bool skip_page_fav_icons,
                GetIconsCallback callback) override;

  // Set info to respond on |GetWebApplicationInfo|.
  void SetRendererWebApplicationInfo(
      std::unique_ptr<WebApplicationInfo> web_app_info);
  // Set arguments to respond on |CheckInstallabilityAndRetrieveManifest|.
  void SetManifest(std::unique_ptr<blink::Manifest> manifest,
                   bool is_installable);
  // Set icons to respond on |GetIcons|.
  void SetIcons(IconsMap icons_map);

  void SetDestructionCallback(base::OnceClosure callback);

  WebApplicationInfo& web_app_info() { return *web_app_info_; }
  bool HasIcons() const { return !icons_map_.empty(); }

 private:
  std::unique_ptr<WebApplicationInfo> web_app_info_;

  std::unique_ptr<blink::Manifest> manifest_;
  bool is_installable_;

  IconsMap icons_map_;
  base::OnceClosure destruction_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestDataRetriever);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_DATA_RETRIEVER_H_
