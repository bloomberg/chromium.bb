// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/installedapp/related_apps_fetcher.h"

#include "base/bind.h"
#include "content/public/common/manifest.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/modules/installedapp/WebRelatedApplication.h"
#include "third_party/WebKit/public/platform/modules/manifest/manifest.mojom.h"
#include "third_party/WebKit/public/platform/modules/manifest/manifest_manager.mojom.h"

namespace content {

RelatedAppsFetcher::RelatedAppsFetcher(
    blink::mojom::ManifestManager* manifest_manager)
    : manifest_manager_(manifest_manager) {}

RelatedAppsFetcher::~RelatedAppsFetcher() {}

void RelatedAppsFetcher::GetManifestRelatedApplications(
    std::unique_ptr<blink::WebCallbacks<
        const blink::WebVector<blink::WebRelatedApplication>&,
        void>> callbacks) {
  manifest_manager_->RequestManifest(
      base::BindOnce(&RelatedAppsFetcher::OnGetManifestForRelatedApplications,
                     base::Unretained(this), base::Passed(&callbacks)));
}

void RelatedAppsFetcher::OnGetManifestForRelatedApplications(
    std::unique_ptr<blink::WebCallbacks<
        const blink::WebVector<blink::WebRelatedApplication>&,
        void>> callbacks,
    const GURL& /*url*/,
    const Manifest& manifest) {
  std::vector<blink::WebRelatedApplication> related_apps;
  for (const auto& relatedApplication : manifest.related_applications) {
    blink::WebRelatedApplication webRelatedApplication;
    webRelatedApplication.platform =
        blink::WebString::FromUTF16(relatedApplication.platform);
    webRelatedApplication.id =
        blink::WebString::FromUTF16(relatedApplication.id);
    if (!relatedApplication.url.is_empty()) {
      webRelatedApplication.url =
          blink::WebString::FromUTF8(relatedApplication.url.spec());
    }
    related_apps.push_back(std::move(webRelatedApplication));
  }
  callbacks->OnSuccess(std::move(related_apps));
}

}  // namespace content
