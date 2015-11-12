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
    : content::WebContentsObserver(web_contents),
      service_(receiving_service),
      clip_result_(CLIP_RESULT_UNPROCESSED) {
  score_.at_top =
      (web_contents->GetRenderWidgetHostView()->GetLastScrollOffset().y() == 0);
  score_.load_completed = !web_contents->IsLoading() && !load_interrupted;
}

ThumbnailingContext::ThumbnailingContext()
  : content::WebContentsObserver(nullptr),
    clip_result_(CLIP_RESULT_UNPROCESSED) {
}

ThumbnailingContext::~ThumbnailingContext() {
}

const scoped_refptr<ThumbnailService>& ThumbnailingContext::service() const {
  return service_;
}

const GURL& ThumbnailingContext::GetURL() const {
  return web_contents()->GetURL();
}

ClipResult ThumbnailingContext::clip_result() const {
  return clip_result_;
}

void ThumbnailingContext::set_clip_result(ClipResult result) {
  clip_result_ = result;
}

gfx::Size ThumbnailingContext::requested_copy_size() {
  return requested_copy_size_;
}

void ThumbnailingContext::set_requested_copy_size(
    const gfx::Size& requested_size) {
  requested_copy_size_ = requested_size;
}

ThumbnailScore ThumbnailingContext::score() const {
  return score_;
}

void ThumbnailingContext::SetBoringScore(double score) {
  score_.boring_score = score;
}

void ThumbnailingContext::SetGoodClipping(bool is_good_clipping) {
  score_.good_clipping = is_good_clipping;
}

}  // namespace thumbnails
