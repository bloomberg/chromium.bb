// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/dom_distiller/distiller_viewer.h"

#include <string>

#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "components/dom_distiller/core/viewer.h"
#include "ios/chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "ios/public/provider/chrome/browser/browser_state/chrome_browser_state.h"
#include "ui/gfx/geometry/size.h"

namespace dom_distiller {

DistillerViewer::DistillerViewer(ios::ChromeBrowserState* browser_state,
                                 const GURL& url,
                                 const DistillationFinishedCallback& callback)
    : url_(url),
      callback_(callback),
      distilled_page_prefs_(new DistilledPagePrefs(browser_state->GetPrefs())) {
  DCHECK(browser_state);
  DCHECK(url.is_valid());
  dom_distiller::DomDistillerService* distillerService =
      dom_distiller::DomDistillerServiceFactory::GetForBrowserState(
          browser_state);

  viewer_handle_ = distillerService->ViewUrl(
      this, distillerService->CreateDefaultDistillerPage(gfx::Size()), url);
}

DistillerViewer::~DistillerViewer() {
}

void DistillerViewer::OnArticleReady(
    const DistilledArticleProto* article_proto) {
  const std::string html = viewer::GetUnsafeArticleHtml(
      article_proto, distilled_page_prefs_->GetTheme(),
      distilled_page_prefs_->GetFontFamily());

  callback_.Run(url_, html);

  // No need to hold on to the ViewerHandle now that distillation is complete.
  viewer_handle_.reset();
}

}  // namespace dom_distiller
