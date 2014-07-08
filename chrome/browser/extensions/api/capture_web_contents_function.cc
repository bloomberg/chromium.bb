// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/capture_web_contents_function.h"

#include "base/base64.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/constants.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"

using content::RenderViewHost;
using content::RenderWidgetHost;
using content::RenderWidgetHostView;
using content::WebContents;

namespace extensions {

CaptureWebContentsFunction::CaptureWebContentsFunction() {
}

CaptureWebContentsFunction::~CaptureWebContentsFunction() {
}

bool CaptureWebContentsFunction::HasPermission() {
  return true;
}

bool CaptureWebContentsFunction::RunAsync() {
  EXTENSION_FUNCTION_VALIDATE(args_);

  context_id_ = extension_misc::kCurrentWindowId;
  args_->GetInteger(0, &context_id_);

  scoped_ptr<ImageDetails> image_details;
  if (args_->GetSize() > 1) {
    base::Value* spec = NULL;
    EXTENSION_FUNCTION_VALIDATE(args_->Get(1, &spec) && spec);
    image_details = ImageDetails::FromValue(*spec);
  }

  if (!IsScreenshotEnabled())
    return false;

  WebContents* contents = GetWebContentsForID(context_id_);
  if (!contents)
    return false;

  // The default format and quality setting used when encoding jpegs.
  const ImageDetails::Format kDefaultFormat = ImageDetails::FORMAT_JPEG;
  const int kDefaultQuality = 90;

  image_format_ = kDefaultFormat;
  image_quality_ = kDefaultQuality;

  if (image_details) {
    if (image_details->format != ImageDetails::FORMAT_NONE)
      image_format_ = image_details->format;
    if (image_details->quality.get())
      image_quality_ = *image_details->quality;
  }

  RenderViewHost* render_view_host = contents->GetRenderViewHost();
  RenderWidgetHostView* view = render_view_host->GetView();
  if (!view) {
    OnCaptureFailure(FAILURE_REASON_VIEW_INVISIBLE);
    return false;
  }
  render_view_host->CopyFromBackingStore(
      gfx::Rect(),
      view->GetViewBounds().size(),
      base::Bind(&CaptureWebContentsFunction::CopyFromBackingStoreComplete,
                 this),
      kN32_SkColorType);
  return true;
}

void CaptureWebContentsFunction::CopyFromBackingStoreComplete(
    bool succeeded,
    const SkBitmap& bitmap) {
  if (succeeded) {
    OnCaptureSuccess(bitmap);
    return;
  }
  OnCaptureFailure(FAILURE_REASON_UNKNOWN);
}

void CaptureWebContentsFunction::OnCaptureSuccess(const SkBitmap& bitmap) {
  std::vector<unsigned char> data;
  SkAutoLockPixels screen_capture_lock(bitmap);
  bool encoded = false;
  std::string mime_type;
  switch (image_format_) {
    case ImageDetails::FORMAT_JPEG:
      encoded = gfx::JPEGCodec::Encode(
          reinterpret_cast<unsigned char*>(bitmap.getAddr32(0, 0)),
          gfx::JPEGCodec::FORMAT_SkBitmap,
          bitmap.width(),
          bitmap.height(),
          static_cast<int>(bitmap.rowBytes()),
          image_quality_,
          &data);
      mime_type = tabs_constants::kMimeTypeJpeg;
      break;
    case ImageDetails::FORMAT_PNG:
      encoded = gfx::PNGCodec::EncodeBGRASkBitmap(
          bitmap,
          true,  // Discard transparency.
          &data);
      mime_type = tabs_constants::kMimeTypePng;
      break;
    default:
      NOTREACHED() << "Invalid image format.";
  }

  if (!encoded) {
    OnCaptureFailure(FAILURE_REASON_ENCODING_FAILED);
    return;
  }

  std::string base64_result;
  base::StringPiece stream_as_string(
      reinterpret_cast<const char*>(vector_as_array(&data)), data.size());

  base::Base64Encode(stream_as_string, &base64_result);
  base64_result.insert(0, base::StringPrintf("data:%s;base64,",
                                             mime_type.c_str()));
  SetResult(new base::StringValue(base64_result));
  SendResponse(true);
}

}  // namespace extensions
