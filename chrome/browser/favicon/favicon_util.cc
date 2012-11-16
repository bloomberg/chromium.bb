// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/favicon_util.h"

#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/select_favicon_frames.h"
#include "chrome/common/icon_messages.h"
#include "content/public/browser/render_view_host.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"

// static
int FaviconUtil::DownloadFavicon(content::RenderViewHost* rvh,
                                 const GURL& url,
                                 int image_size) {
  static int id = 0;
  rvh->Send(new IconMsg_DownloadFavicon(rvh->GetRoutingID(), ++id, url,
            image_size));
  return id;
}
// static
gfx::Image FaviconUtil::SelectFaviconFramesFromPNGs(
      const std::vector<history::FaviconBitmapResult>& png_data,
      const std::vector<ui::ScaleFactor> scale_factors,
      int favicon_size) {
  std::vector<SkBitmap> bitmaps;
  for (size_t i = 0; i < png_data.size(); ++i) {
    if (!png_data[i].is_valid())
      continue;

    SkBitmap bitmap;
    if (gfx::PNGCodec::Decode(png_data[i].bitmap_data->front(),
                              png_data[i].bitmap_data->size(),
                              &bitmap)) {
      bitmaps.push_back(bitmap);
    }
  }

  if (bitmaps.empty())
    return gfx::Image();

  gfx::ImageSkia resized_image_skia = SelectFaviconFrames(bitmaps,
      ui::GetSupportedScaleFactors(), favicon_size, NULL);
  return gfx::Image(resized_image_skia);
}
