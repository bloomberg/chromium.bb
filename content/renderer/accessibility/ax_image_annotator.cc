// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/ax_image_annotator.h"

#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/app/strings/grit/content_strings.h"
#include "content/public/common/content_client.h"
#include "content/renderer/render_frame_impl.h"
#include "crypto/sha2.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace content {

AXImageAnnotator::AXImageAnnotator(
    RenderAccessibilityImpl* const render_accessibility,
    image_annotation::mojom::AnnotatorPtr annotator_ptr)
    : render_accessibility_(render_accessibility),
      annotator_ptr_(std::move(annotator_ptr)),
      weak_factory_(this) {
  DCHECK(render_accessibility_);
}

AXImageAnnotator::~AXImageAnnotator() {}

void AXImageAnnotator::Destroy() {
  MarkAllImagesDirty();
}

std::string AXImageAnnotator::GetImageAnnotation(
    blink::WebAXObject& image) const {
  DCHECK(!image.IsDetached());
  const auto lookup = image_annotations_.find(image.AxID());
  if (lookup != image_annotations_.end())
    return lookup->second.annotation();
  return std::string();
}

ax::mojom::ImageAnnotationStatus AXImageAnnotator::GetImageAnnotationStatus(
    blink::WebAXObject& image) const {
  DCHECK(!image.IsDetached());
  const auto lookup = image_annotations_.find(image.AxID());
  if (lookup != image_annotations_.end())
    return lookup->second.status();
  return ax::mojom::ImageAnnotationStatus::kNone;
}

bool AXImageAnnotator::HasAnnotationInCache(blink::WebAXObject& image) const {
  DCHECK(!image.IsDetached());
  if (!HasImageInCache(image))
    return false;
  return image_annotations_.at(image.AxID()).HasAnnotation();
}

bool AXImageAnnotator::HasImageInCache(const blink::WebAXObject& image) const {
  DCHECK(!image.IsDetached());
  return base::ContainsKey(image_annotations_, image.AxID());
}

void AXImageAnnotator::OnImageAdded(blink::WebAXObject& image) {
  DCHECK(!image.IsDetached());
  DCHECK(!base::ContainsKey(image_annotations_, image.AxID()));
  const std::string image_id = GenerateImageSourceId(image);
  if (image_id.empty())
    return;

  image_annotations_.emplace(image.AxID(), image);
  ImageInfo& image_info = image_annotations_.at(image.AxID());
  // Fetch image annotation.
  annotator_ptr_->AnnotateImage(
      image_id, image_info.GetImageProcessor(),
      base::BindOnce(&AXImageAnnotator::OnImageAnnotated,
                     weak_factory_.GetWeakPtr(), image));
  VLOG(1) << "Requesting annotation for " << image_id << " from page "
          << GetDocumentUrl();
}

void AXImageAnnotator::OnImageUpdated(blink::WebAXObject& image) {
  DCHECK(!image.IsDetached());
  DCHECK(base::ContainsKey(image_annotations_, image.AxID()));
  const std::string image_id = GenerateImageSourceId(image);
  if (image_id.empty())
    return;

  ImageInfo& image_info = image_annotations_.at(image.AxID());
  // Update annotation.
  annotator_ptr_->AnnotateImage(
      image_id, image_info.GetImageProcessor(),
      base::BindOnce(&AXImageAnnotator::OnImageAnnotated,
                     weak_factory_.GetWeakPtr(), image));
}

void AXImageAnnotator::OnImageRemoved(blink::WebAXObject& image) {
  DCHECK(!image.IsDetached());
  auto lookup = image_annotations_.find(image.AxID());
  if (lookup == image_annotations_.end()) {
    NOTREACHED() << "Removing an image that has not been added.";
    return;
  }
  image_annotations_.erase(lookup);
}

#if defined(CONTENT_IMPLEMENTATION)
ContentClient* AXImageAnnotator::GetContentClient() const {
  return content::GetContentClient();
}
#else
ContentClient* AXImageAnnotator::GetContentClient() const {
  return nullptr;
}
#endif  // defined(CONTENT_IMPLEMENTATION)

std::string AXImageAnnotator::GenerateImageSourceId(
    const blink::WebAXObject& image) const {
  DCHECK(render_accessibility_);
  DCHECK(!image.IsDetached());
  const std::string document_url =
      render_accessibility_->GetMainDocument().Url().GetString().Utf8();
  const std::string image_src = image.Url().GetString().Utf8();
  if (document_url.empty() || image_src.empty())
    return std::string();

  // The |image_src| might be a URL that is relative to the document's URL.
  const GURL image_url = GURL(document_url).Resolve(image_src);
  if (!image_url.is_valid())
    return std::string();

  // If |image_url| appears to be publicly reachable, return the URL as the
  // image source ID.
  if (image_url.SchemeIsHTTPOrHTTPS())
    return image_url.spec();

  // If |image_url| is not publicly reachable, return a hash of |image_url|.
  // Scheme could be "data", "javascript", "ftp", "file", etc.
  const std::string& content = image_url.GetContent();
  if (content.empty())
    return std::string();
  std::string source_id;
  base::Base64Encode(crypto::SHA256HashString(content), &source_id);
  return source_id;
}

void AXImageAnnotator::MarkAllImagesDirty() {
  for (auto& key_value : image_annotations_) {
    blink::WebAXObject image = blink::WebAXObject::FromWebDocumentByID(
        render_accessibility_->GetMainDocument(), key_value.first);
    if (!image.IsDetached())
      render_accessibility_->MarkWebAXObjectDirty(image, false /* subtree */);
  }
  image_annotations_.clear();
}

AXImageAnnotator::ImageInfo::ImageInfo(const blink::WebAXObject& image)
    : image_processor_(
          base::BindRepeating(&AXImageAnnotator::GetImageData, image)),
      status_(ax::mojom::ImageAnnotationStatus::kAnnotationPending),
      annotation_(base::nullopt) {}

AXImageAnnotator::ImageInfo::~ImageInfo() = default;

image_annotation::mojom::ImageProcessorPtr
AXImageAnnotator::ImageInfo::GetImageProcessor() {
  return image_processor_.GetPtr();
}

bool AXImageAnnotator::ImageInfo::HasAnnotation() const {
  switch (status_) {
    case ax::mojom::ImageAnnotationStatus::kNone:
    case ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation:
    // The user hasn't requested an annotation yet, or a previously pending
    // annotation request had been cancelled.
    case ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation:
    case ax::mojom::ImageAnnotationStatus::kAnnotationPending:
      return false;
    case ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded:
      DCHECK(annotation_.has_value());
      return true;
    case ax::mojom::ImageAnnotationStatus::kAnnotationEmpty:
    // Image has been classified as adult content.
    case ax::mojom::ImageAnnotationStatus::kAnnotationAdult:
    case ax::mojom::ImageAnnotationStatus::kAnnotationProcessFailed:
      DCHECK(!annotation_.has_value());
      return true;
  }
}

// static
SkBitmap AXImageAnnotator::GetImageData(const blink::WebAXObject& image) {
  if (image.IsDetached())
    return SkBitmap();
  blink::WebNode node = image.GetNode();
  if (node.IsNull() || !node.IsElementNode())
    return SkBitmap();
  blink::WebElement element = node.To<blink::WebElement>();
  VLOG(1) << "Uploading pixels for " << element.ImageContents().width() << " x "
          << element.ImageContents().height() << " image";
  return element.ImageContents();
}

void AXImageAnnotator::OnImageAnnotated(
    const blink::WebAXObject& image,
    image_annotation::mojom::AnnotateImageResultPtr result) {
  if (!base::ContainsKey(image_annotations_, image.AxID()))
    return;

  if (image.IsDetached()) {
    image_annotations_.at(image.AxID())
        .set_status(ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation);
    // We should not mark dirty a detached object.
    return;
  }

  if (result->is_error_code()) {
    DLOG(WARNING) << "Image annotation error.";
    switch (result->get_error_code()) {
      case image_annotation::mojom::AnnotateImageError::kCanceled:
        image_annotations_.at(image.AxID())
            .set_status(
                ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation);
        break;
      case image_annotation::mojom::AnnotateImageError::kFailure:
        image_annotations_.at(image.AxID())
            .set_status(
                ax::mojom::ImageAnnotationStatus::kAnnotationProcessFailed);
        break;
      case image_annotation::mojom::AnnotateImageError::kAdult:
        image_annotations_.at(image.AxID())
            .set_status(ax::mojom::ImageAnnotationStatus::kAnnotationAdult);
        break;
    }
    render_accessibility_->MarkWebAXObjectDirty(image, false /* subtree */);
    return;
  }

  if (!result->is_annotations()) {
    DLOG(WARNING) << "No image annotation results.";
    image_annotations_.at(image.AxID())
        .set_status(ax::mojom::ImageAnnotationStatus::kAnnotationEmpty);
    render_accessibility_->MarkWebAXObjectDirty(image, false /* subtree */);
    return;
  }

  std::vector<std::string> contextualized_strings;
  for (const mojo::InlinedStructPtr<image_annotation::mojom::Annotation>&
           annotation : result->get_annotations()) {
    int message_id = 0;
    switch (annotation->type) {
      case image_annotation::mojom::AnnotationType::kOcr:
        message_id = IDS_AX_IMAGE_ANNOTATION_OCR_CONTEXT;
        break;
      case image_annotation::mojom::AnnotationType::kCaption:
      case image_annotation::mojom::AnnotationType::kLabel:
        message_id = IDS_AX_IMAGE_ANNOTATION_DESCRIPTION_CONTEXT;
        break;
    }

    // Skip unrecognized annotation types.
    if (message_id == 0)
      continue;

    if (annotation->text.empty())
      continue;

    if (GetContentClient()) {
      contextualized_strings.push_back(
          base::UTF16ToUTF8(GetContentClient()->GetLocalizedString(
              message_id, base::UTF8ToUTF16(annotation->text))));
    } else {
      contextualized_strings.emplace_back(annotation->text);
    }
  }

  if (contextualized_strings.empty()) {
    image_annotations_.at(image.AxID())
        .set_status(ax::mojom::ImageAnnotationStatus::kAnnotationEmpty);
    render_accessibility_->MarkWebAXObjectDirty(image, false /* subtree */);
    return;
  }

  image_annotations_.at(image.AxID())
      .set_status(ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded);
  // TODO(accessibility): join two sentences together in a more i18n-friendly
  // way. Since this is intended for a screen reader, though, a period
  // probably works in almost all languages.
  std::string contextualized_string =
      base::JoinString(contextualized_strings, ". ");
  image_annotations_.at(image.AxID()).set_annotation(contextualized_string);
  render_accessibility_->MarkWebAXObjectDirty(image, false /* subtree */);

  VLOG(1) << "Annotating image on page " << GetDocumentUrl() << " - "
          << contextualized_string;
}

std::string AXImageAnnotator::GetDocumentUrl() const {
  const blink::WebLocalFrame* frame =
      render_accessibility_->render_frame()->GetWebFrame();
  return frame->GetDocument().Url().GetString().Utf8();
}

}  // namespace content
