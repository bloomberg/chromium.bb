// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnails/thumbnailing_context.h"

#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"

namespace thumbnails {

ThumbnailingContext::ThumbnailingContext(content::WebContents* web_contents,
                                         ThumbnailService* receiving_service,
                                         bool load_interrupted)
    : service(receiving_service),
      url(web_contents->GetLastCommittedURL()),
      clip_result(CLIP_RESULT_UNPROCESSED) {
  score.at_top =
      (web_contents->GetRenderWidgetHostView()->GetLastScrollOffset().y() == 0);
  score.load_completed = !web_contents->IsLoading() && !load_interrupted;
}

ThumbnailingContext::ThumbnailingContext()
  : clip_result(CLIP_RESULT_UNPROCESSED) {
}

ThumbnailingContext::~ThumbnailingContext() {
}

}  // namespace thumbnails
