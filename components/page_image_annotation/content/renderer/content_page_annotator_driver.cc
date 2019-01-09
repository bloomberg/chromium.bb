// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_image_annotation/content/renderer/content_page_annotator_driver.h"

#include "content/public/renderer/render_frame.h"

namespace page_image_annotation {

ContentPageAnnotatorDriver::ContentPageAnnotatorDriver(
    content::RenderFrame* const render_frame)
    : RenderFrameObserver(render_frame),
      RenderFrameObserverTracker<ContentPageAnnotatorDriver>(render_frame) {}

ContentPageAnnotatorDriver::~ContentPageAnnotatorDriver() {}

// static
ContentPageAnnotatorDriver* ContentPageAnnotatorDriver::GetOrCreate(
    content::RenderFrame* const render_frame) {
  ContentPageAnnotatorDriver* const existing = Get(render_frame);
  if (existing)
    return existing;

  return new ContentPageAnnotatorDriver(render_frame);
}

PageAnnotator& ContentPageAnnotatorDriver::GetPageAnnotator() {
  return page_annotator_;
}

void ContentPageAnnotatorDriver::DidCommitProvisionalLoad(
    const bool /* is_same_document_navigation */,
    const ui::PageTransition /* transition */) {
  // TODO(crbug.com/915076): schedule repeated DOM traversals to track image
  //                         addition / modification / removal.
}

void ContentPageAnnotatorDriver::OnDestruct() {
  // TODO(crbug.com/915076): cancel DOM traversal.

  delete this;
}

}  // namespace page_image_annotation
