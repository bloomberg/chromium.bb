// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_AX_IMAGE_ANNOTATOR_H_
#define CONTENT_RENDERER_ACCESSIBILITY_AX_IMAGE_ANNOTATOR_H_

#include <string>
#include <unordered_map>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "base/optional.h"
#include "content/renderer/accessibility/render_accessibility_impl.h"
#include "services/image_annotation/public/cpp/image_processor.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

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

  void Destroy();

  std::string GetImageAnnotation(blink::WebAXObject& image) const;
  bool HasAnnotationInCache(blink::WebAXObject& image) const;
  bool HasImageInCache(const blink::WebAXObject& image) const;

  void OnImageAdded(blink::WebAXObject& image);
  void OnImageUpdated(blink::WebAXObject& image);
  void OnImageRemoved(blink::WebAXObject& image);

 private:
  // Keeps track of the image data and the automatic annotation for each image.
  class ImageInfo final {
   public:
    ImageInfo(const blink::WebAXObject& image);
    virtual ~ImageInfo();

    image_annotation::mojom::ImageProcessorPtr GetImageProcessor();
    bool HasAnnotation() const;

    std::string annotation() const {
      DCHECK(annotation_.has_value());
      return annotation_.value_or("");
    }

    void set_annotation(std::string annotation) { annotation_ = annotation; }

   private:
    image_annotation::ImageProcessor image_processor_;
    base::Optional<std::string> annotation_;
  };

  // Given the URL of the main document and the src attribute of an image,
  // generates a unique identifier for the image that could be provided to the
  // image annotation service.
  static std::string GenerateImageSourceId(const std::string& document_url,
                                           const std::string& image_src);

  // Retrieves the image data from the renderer.
  static SkBitmap GetImageData(const blink::WebAXObject& image);

  // Removes the automatic image annotations from all images.
  void MarkAllImagesDirty();

  // Gets called when an image gets annotated by the image annotation service.
  void OnImageAnnotated(const blink::WebAXObject& image,
                        image_annotation::mojom::AnnotateImageResultPtr result);

  // Weak, owns us.
  RenderAccessibilityImpl* const render_accessibility_;

  // A pointer to the automatic image annotation service.
  image_annotation::mojom::AnnotatorPtr annotator_ptr_;

  // Keeps track of the image data and the automatic annotations for each image.
  //
  // The key is retrieved using WebAXObject::AxID().
  std::unordered_map<int, ImageInfo> image_annotations_;

  // This member needs to be last because it should destructed first.
  base::WeakPtrFactory<AXImageAnnotator> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AXImageAnnotator);
};

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_AX_IMAGE_ANNOTATOR_H_
