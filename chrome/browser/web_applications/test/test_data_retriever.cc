// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_data_retriever.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/common/web_application_info.h"
#include "third_party/blink/public/common/manifest/manifest.h"

namespace web_app {

TestDataRetriever::TestDataRetriever() = default;

TestDataRetriever::~TestDataRetriever() {
  if (destruction_callback_)
    std::move(destruction_callback_).Run();
}

void TestDataRetriever::GetWebApplicationInfo(
    content::WebContents* web_contents,
    GetWebApplicationInfoCallback callback) {
  DCHECK(web_contents);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(web_app_info_)));
}

void TestDataRetriever::CheckInstallabilityAndRetrieveManifest(
    content::WebContents* web_contents,
    CheckInstallabilityCallback callback) {
  if (manifest_ == nullptr) {
    WebAppDataRetriever::CheckInstallabilityAndRetrieveManifest(
        web_contents, std::move(callback));
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), *manifest_, is_installable_));
}

void TestDataRetriever::GetIcons(content::WebContents* web_contents,
                                 const std::vector<GURL>& icon_urls,
                                 bool skip_page_fav_icons,
                                 GetIconsCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(icons_map_)));
  icons_map_.clear();
}

void TestDataRetriever::SetRendererWebApplicationInfo(
    std::unique_ptr<WebApplicationInfo> web_app_info) {
  web_app_info_ = std::move(web_app_info);
}

void TestDataRetriever::SetManifest(std::unique_ptr<blink::Manifest> manifest,
                                    bool is_installable) {
  manifest_ = std::move(manifest);
  is_installable_ = is_installable;
}

void TestDataRetriever::SetIcons(IconsMap icons_map) {
  icons_map_ = std::move(icons_map);
}

void TestDataRetriever::SetDestructionCallback(base::OnceClosure callback) {
  destruction_callback_ = std::move(callback);
}

}  // namespace web_app
