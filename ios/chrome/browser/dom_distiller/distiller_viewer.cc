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
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "ui/gfx/geometry/size.h"

namespace dom_distiller {

DistillerViewer::DistillerViewer(ios::ChromeBrowserState* browser_state,
                                 const GURL& url,
                                 const DistillationFinishedCallback& callback)
    : DomDistillerRequestViewBase(
          new DistilledPagePrefs(browser_state->GetPrefs())),
      url_(url),
      callback_(callback) {
  DCHECK(browser_state);
  DCHECK(url.is_valid());
  dom_distiller::DomDistillerService* distillerService =
      dom_distiller::DomDistillerServiceFactory::GetForBrowserState(
          browser_state);

  scoped_ptr<ViewerHandle> viewer_handle = distillerService->ViewUrl(
      this, distillerService->CreateDefaultDistillerPage(gfx::Size()), url);

  TakeViewerHandle(std::move(viewer_handle));
}

DistillerViewer::~DistillerViewer() {
}

void DistillerViewer::OnArticleReady(
    const dom_distiller::DistilledArticleProto* article_proto) {
  DomDistillerRequestViewBase::OnArticleReady(article_proto);

  const std::string html = viewer::GetUnsafeArticleTemplateHtml(
      url_.spec(), distilled_page_prefs_->GetTheme(),
      distilled_page_prefs_->GetFontFamily());

  std::string html_and_script(html);
  html_and_script +=
      "<script> distiller_on_ios = true; " + js_buffer_ + "</script>";
  callback_.Run(url_, html_and_script);
}

void DistillerViewer::SendJavaScript(const std::string& buffer) {
  js_buffer_ += buffer;
}

}  // namespace dom_distiller
