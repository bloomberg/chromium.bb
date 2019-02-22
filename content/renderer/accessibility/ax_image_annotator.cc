// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/ax_image_annotator.h"

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
#include "third_party/blink/public/web/web_node.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace content {

AXImageAnnotator::AXImageAnnotator(
    RenderAccessibilityImpl* const render_accessibility)
    : render_accessibility_(render_accessibility), weak_factory_(this) {
  DCHECK(render_accessibility_);
  render_accessibility_->render_frame()->GetRemoteInterfaces()->GetInterface(
      mojo::MakeRequest(&annotator_ptr_));
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
  std::string document_url =
      render_accessibility_->GetMainDocument().Url().GetString().Utf8();
  std::string image_src = image.Url().GetString().Utf8();
  std::string image_id = GenerateImageSourceId(document_url, image_src);
  if (image_id.empty())
    return;

  image_annotations_.emplace(image.AxID(), image);
  ImageInfo& image_info = image_annotations_.at(image.AxID());
  // Fetch image annotation.
  annotator_ptr_->AnnotateImage(
      image_id, image_info.GetImageProcessor(),
      base::BindOnce(&AXImageAnnotator::OnImageAnnotated,
                     weak_factory_.GetWeakPtr(), image));
}

void AXImageAnnotator::OnImageUpdated(blink::WebAXObject& image) {
  DCHECK(!image.IsDetached());
  DCHECK(base::ContainsKey(image_annotations_, image.AxID()));
  std::string document_url =
      render_accessibility_->GetMainDocument().Url().GetString().Utf8();
  std::string image_src = image.Url().GetString().Utf8();
  std::string image_id = GenerateImageSourceId(document_url, image_src);
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

void AXImageAnnotator::MarkAllImagesDirty() {
  for (auto& key_value : image_annotations_) {
    blink::WebAXObject image = blink::WebAXObject::FromWebDocumentByID(
        render_accessibility_->GetMainDocument(), key_value.first);
    DCHECK(!image.IsDetached());
    render_accessibility_->MarkWebAXObjectDirty(image, false /* subtree */);
  }
  image_annotations_.clear();
}

AXImageAnnotator::ImageInfo::ImageInfo(const blink::WebAXObject& image)
    : image_processor_(
          base::BindRepeating(&AXImageAnnotator::GetImageData, image)),
      annotation_(base::nullopt) {}

AXImageAnnotator::ImageInfo::~ImageInfo() = default;

image_annotation::mojom::ImageProcessorPtr
AXImageAnnotator::ImageInfo::GetImageProcessor() {
  return image_processor_.GetPtr();
}

bool AXImageAnnotator::ImageInfo::HasAnnotation() const {
  return annotation_.has_value();
}

// static
std::string AXImageAnnotator::GenerateImageSourceId(
    const std::string& document_url,
    const std::string& image_src) {
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

// static
SkBitmap AXImageAnnotator::GetImageData(const blink::WebAXObject& image) {
  if (image.IsDetached())
    return SkBitmap();
  blink::WebNode node = image.GetNode();
  if (node.IsNull() || !node.IsElementNode())
    return SkBitmap();
  blink::WebElement element = node.To<blink::WebElement>();
  return element.ImageContents();
}

void AXImageAnnotator::OnImageAnnotated(
    const blink::WebAXObject& image,
    image_annotation::mojom::AnnotateImageResultPtr result) {
  if (image.IsDetached())
    return;
  if (!base::ContainsKey(image_annotations_, image.AxID()))
    return;
  // TODO(nektar): Set the image annotation status on this image to Error.
  if (result->is_error_code())
    return;
  if (!result->is_ocr_text()) {
    DLOG(WARNING) << "Unrecognized image annotation result.";
    return;
  }

  if (result->get_ocr_text().empty())
    return;

  auto contextualized_string = GetContentClient()->GetLocalizedString(
      IDS_AX_IMAGE_ANNOTATION_OCR_CONTEXT,
      base::UTF8ToUTF16(result->get_ocr_text()));

  image_annotations_.at(image.AxID())
      .set_annotation(base::UTF16ToUTF8(contextualized_string));
  render_accessibility_->MarkWebAXObjectDirty(image, false /* subtree */);
}

}  // namespace content
