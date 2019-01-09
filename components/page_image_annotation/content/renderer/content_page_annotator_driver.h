// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_IMAGE_ANNOTATION_CONTENT_RENDERER_CONTENT_PAGE_ANNOTATOR_DRIVER_H_
#define COMPONENTS_PAGE_IMAGE_ANNOTATION_CONTENT_RENDERER_CONTENT_PAGE_ANNOTATOR_DRIVER_H_

#include "base/macros.h"
#include "components/page_image_annotation/core/page_annotator.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "ui/base/page_transition_types.h"

namespace page_image_annotation {

class ContentPageAnnotatorDriver
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<ContentPageAnnotatorDriver> {
 public:
  ~ContentPageAnnotatorDriver() override;

  static ContentPageAnnotatorDriver* GetOrCreate(
      content::RenderFrame* render_frame);

  PageAnnotator& GetPageAnnotator();

 private:
  // We delete ourselves on frame destruction, so disallow construction on the
  // stack.
  ContentPageAnnotatorDriver(content::RenderFrame* render_frame);

  // content::RenderFrameObserver:
  void DidCommitProvisionalLoad(bool is_same_document_navigation,
                                ui::PageTransition transition) override;
  void OnDestruct() override;

  PageAnnotator page_annotator_;

  DISALLOW_COPY_AND_ASSIGN(ContentPageAnnotatorDriver);
};

}  // namespace page_image_annotation

#endif  //  COMPONENTS_PAGE_IMAGE_ANNOTATION_CONTENT_RENDERER_CONTENT_PAGE_ANNOTATOR_DRIVER_H_
