// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/ax_image_annotator.h"

#include "base/stl_util.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_document.h"

namespace content {

AXImageAnnotator::AXImageAnnotator(
    RenderAccessibilityImpl* const render_accessibility)
    : render_accessibility_(render_accessibility) {
  DCHECK(render_accessibility_);
}

AXImageAnnotator::~AXImageAnnotator() {
  MarkAllImagesDirty();
}

std::string AXImageAnnotator::GetImageAnnotation(
    blink::WebAXObject& image) const {
  DCHECK(!image.IsDetached());
  const auto lookup = image_annotations_.find(image.AxID());
  if (lookup != image_annotations_.end())
    return lookup->second;
  return {};
}

void AXImageAnnotator::OnImageAdded(blink::WebAXObject& image) {
  DCHECK(!image.IsDetached());
  DCHECK(!base::ContainsKey(image_annotations_, image.AxID()));
  // Fetch image annotation.
  image_annotations_.emplace(image.AxID(), std::string());
  render_accessibility_->MarkWebAXObjectDirty(image, false /* subtree */);
}

void AXImageAnnotator::OnImageUpdated(blink::WebAXObject& image) {
  DCHECK(!image.IsDetached());
  DCHECK(base::ContainsKey(image_annotations_, image.AxID()));
  // Update annotation.
  image_annotations_[image.AxID()] = std::string();
  render_accessibility_->MarkWebAXObjectDirty(image, false /* subtree */);
}

void AXImageAnnotator::OnImageRemoved(blink::WebAXObject& image) {
  DCHECK(!image.IsDetached());
  auto lookup = image_annotations_.find(image.AxID());
  if (lookup == image_annotations_.end()) {
    NOTREACHED() << "Removing an image that has not been added.";
    return;
  }
  image_annotations_.erase(lookup);
  render_accessibility_->MarkWebAXObjectDirty(image, false /* subtree */);
}

void AXImageAnnotator::MarkAllImagesDirty() {
  for (auto& key_value : image_annotations_) {
    blink::WebAXObject image = blink::WebAXObject::FromWebDocumentByID(
        render_accessibility_->GetMainDocument(), key_value.first);
    DCHECK(!image.IsDetached());
    render_accessibility_->MarkWebAXObjectDirty(image, false /* subtree */);
  }
  image_annotations_.clear();
}

}  // namespace content
