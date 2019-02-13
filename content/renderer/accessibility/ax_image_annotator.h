// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_AX_IMAGE_ANNOTATOR_H_
#define CONTENT_RENDERER_ACCESSIBILITY_AX_IMAGE_ANNOTATOR_H_

#include <string>
#include <unordered_map>

#include "base/macros.h"
#include "base/observer_list_types.h"
#include "content/renderer/accessibility/render_accessibility_impl.h"

namespace blink {

class WebAXObject;

}  // namespace blink

namespace content {

// This class gets notified that certain images have been added, removed or
// updated on a page. This class is then responsible for retrieving the
// automatic label for all images and notifying the RenderAccessibility that
// owns it to update the relevant image annotations.
class AXImageAnnotator final : public base::CheckedObserver {
 public:
  AXImageAnnotator(RenderAccessibilityImpl* const render_accessibility);
  ~AXImageAnnotator() override;

  std::string GetImageAnnotation(blink::WebAXObject& image) const;

  void OnImageAdded(blink::WebAXObject& image);
  void OnImageUpdated(blink::WebAXObject& image);
  void OnImageRemoved(blink::WebAXObject& image);

 private:
  // Removes the automatic image annotations from all images.
  void MarkAllImagesDirty();

  // Weak, owns us.
  RenderAccessibilityImpl* const render_accessibility_;

  // Keeps track of all the automatic annotations for each image.
  //
  // The key is retrieved using WebAXObject::AxID().
  std::unordered_map<int, std::string> image_annotations_;

  DISALLOW_COPY_AND_ASSIGN(AXImageAnnotator);
};

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_AX_IMAGE_ANNOTATOR_H_
