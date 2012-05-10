// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_snapshot/window_snapshot.h"

#include "base/logging.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/window.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/rect.h"

namespace browser {

bool GrabWindowSnapshot(gfx::NativeWindow window,
                        std::vector<unsigned char>* png_representation,
                        const gfx::Rect& snapshot_bounds) {
  ui::Compositor* compositor = window->layer()->GetCompositor();

  gfx::Rect read_pixels_bounds = snapshot_bounds;

  // When not in compact mode we must take into account the window's position on
  // the desktop.
  read_pixels_bounds.set_origin(
      snapshot_bounds.origin().Add(window->bounds().origin()));
  gfx::Rect read_pixels_bounds_in_pixel =
      ui::ConvertRectToPixel(window->layer(), read_pixels_bounds_in_pixel);

  DCHECK_GE(compositor->size().width(), read_pixels_bounds_in_pixel.right());
  DCHECK_GE(compositor->size().height(), read_pixels_bounds_in_pixel.bottom());
  DCHECK_LE(0, read_pixels_bounds.x());
  DCHECK_LE(0, read_pixels_bounds.y());

  SkBitmap bitmap;
  if (!compositor->ReadPixels(&bitmap, read_pixels_bounds_in_pixel))
    return false;

  unsigned char* pixels = reinterpret_cast<unsigned char*>(bitmap.getPixels());

  gfx::PNGCodec::Encode(pixels, gfx::PNGCodec::FORMAT_BGRA,
                        read_pixels_bounds_in_pixel.size(),
                        bitmap.rowBytes(), true,
                        std::vector<gfx::PNGCodec::Comment>(),
                        png_representation);
  return true;
}

}  // namespace browser
