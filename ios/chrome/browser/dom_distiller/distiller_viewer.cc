// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/dom_distiller/distiller_viewer.h"

#include <string>
#include <utility>

#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/dom_distiller_request_view_base.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "components/dom_distiller/core/viewer.h"
#include "ui/gfx/geometry/size.h"

namespace dom_distiller {

DistillerViewerImpl::DistillerViewerImpl(
    dom_distiller::DomDistillerService* distillerService,
    PrefService* prefs,
    const GURL& url,
    const DistillationFinishedCallback& callback)
    : DistillerViewer(distillerService, prefs), url_(url), callback_(callback) {
  DCHECK(distillerService);
  DCHECK(url.is_valid());

  std::unique_ptr<ViewerHandle> viewer_handle = distillerService->ViewUrl(
      this, distillerService->CreateDefaultDistillerPage(gfx::Size()), url);

  TakeViewerHandle(std::move(viewer_handle));
}

DistillerViewerImpl::~DistillerViewerImpl() {}

void DistillerViewerImpl::OnArticleReady(
    const dom_distiller::DistilledArticleProto* article_proto) {
  DomDistillerRequestViewBase::OnArticleReady(article_proto);

  std::vector<ImageInfo> images;
  for (int i = 0; i < article_proto->pages(0).image_size(); i++) {
    auto image = article_proto->pages(0).image(i);
    images.push_back({GURL(image.url()), image.data()});
  }

  const std::string html = viewer::GetUnsafeArticleTemplateHtml(
      url_.spec(), distilled_page_prefs_->GetTheme(),
      distilled_page_prefs_->GetFontFamily());

  std::string html_and_script(html);
  html_and_script +=
      "<script> distiller_on_ios = true; " + js_buffer_ + "</script>";
  callback_.Run(url_, html_and_script, images);
}

void DistillerViewerImpl::SendJavaScript(const std::string& buffer) {
  js_buffer_ += buffer;
}

}  // namespace dom_distiller
