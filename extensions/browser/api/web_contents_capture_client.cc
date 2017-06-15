// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_contents_capture_client.h"

#include "base/base64.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/constants.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"

using content::RenderWidgetHost;
using content::RenderWidgetHostView;
using content::WebContents;

namespace extensions {

using api::extension_types::ImageDetails;

bool WebContentsCaptureClient::CaptureAsync(
    WebContents* web_contents,
    const ImageDetails* image_details,
    const content::ReadbackRequestCallback callback) {
  if (!web_contents)
    return false;

  if (!IsScreenshotEnabled())
    return false;

  // The default format and quality setting used when encoding jpegs.
  const api::extension_types::ImageFormat kDefaultFormat =
      api::extension_types::IMAGE_FORMAT_JPEG;
  const int kDefaultQuality = 90;

  image_format_ = kDefaultFormat;
  image_quality_ = kDefaultQuality;

  if (image_details) {
    if (image_details->format != api::extension_types::IMAGE_FORMAT_NONE)
      image_format_ = image_details->format;
    if (image_details->quality.get())
      image_quality_ = *image_details->quality;
  }

  // TODO(miu): Account for fullscreen render widget?  http://crbug.com/419878
  RenderWidgetHostView* const view = web_contents->GetRenderWidgetHostView();
  if (!view) {
    OnCaptureFailure(FAILURE_REASON_VIEW_INVISIBLE);
    return false;
  }
  view->CopyFromSurface(gfx::Rect(),  // Copy entire surface area.
                        gfx::Size(),  // Result contains device-level detail.
                        callback, kN32_SkColorType);
  return true;
}

void WebContentsCaptureClient::CopyFromSurfaceComplete(
    const SkBitmap& bitmap,
    content::ReadbackResponse response) {
  if (response == content::READBACK_SUCCESS) {
    OnCaptureSuccess(bitmap);
    return;
  }
  // TODO(wjmaclean): Improve error reporting. Why aren't we passing more
  // information here?
  std::string reason;
  switch (response) {
    case content::READBACK_FAILED:
      reason = "READBACK_FAILED";
      break;
    case content::READBACK_SURFACE_UNAVAILABLE:
      reason = "READBACK_SURFACE_UNAVAILABLE";
      break;
    case content::READBACK_BITMAP_ALLOCATION_FAILURE:
      reason = "READBACK_BITMAP_ALLOCATION_FAILURE";
      break;
    default:
      reason = "<unknown>";
  }
  OnCaptureFailure(FAILURE_REASON_UNKNOWN);
}

// TODO(wjmaclean) can this be static?
bool WebContentsCaptureClient::EncodeBitmap(const SkBitmap& bitmap,
                                            std::string* base64_result) {
  DCHECK(base64_result);
  std::vector<unsigned char> data;
  const bool should_discard_alpha = !ClientAllowsTransparency();
  bool encoded = false;
  std::string mime_type;
  switch (image_format_) {
    case api::extension_types::IMAGE_FORMAT_JPEG:
      encoded = gfx::JPEGCodec::Encode(bitmap, image_quality_, &data);
      mime_type = kMimeTypeJpeg;
      break;
    case api::extension_types::IMAGE_FORMAT_PNG:
      encoded = gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, should_discard_alpha,
                                                  &data);
      mime_type = kMimeTypePng;
      break;
    default:
      NOTREACHED() << "Invalid image format.";
  }

  if (!encoded)
    return false;

  base::StringPiece stream_as_string(reinterpret_cast<const char*>(data.data()),
                                     data.size());

  base::Base64Encode(stream_as_string, base64_result);
  base64_result->insert(
      0, base::StringPrintf("data:%s;base64,", mime_type.c_str()));

  return true;
}

}  // namespace extensions
