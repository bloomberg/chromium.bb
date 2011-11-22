// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_snapshot/window_snapshot.h"

#include "base/logging.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/window.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/rect.h"

namespace browser {

bool GrabWindowSnapshot(gfx::NativeWindow window,
                        std::vector<unsigned char>* png_representation,
                        const gfx::Rect& snapshot_bounds) {
  ui::Compositor* compositor = window->layer()->GetCompositor();

  gfx::Rect desktop_snapshot_bounds = gfx::Rect(
      snapshot_bounds.origin().Add(window->bounds().origin()),
      snapshot_bounds.size());

  DCHECK_LE(desktop_snapshot_bounds.right() , compositor->size().width());
  DCHECK_LE(desktop_snapshot_bounds.bottom(), compositor->size().height());

  SkBitmap bitmap;
  if (!compositor->ReadPixels(&bitmap, desktop_snapshot_bounds))
    return false;

  unsigned char* pixels = reinterpret_cast<unsigned char*>(bitmap.getPixels());

  gfx::PNGCodec::Encode(pixels, gfx::PNGCodec::FORMAT_BGRA,
                        snapshot_bounds.size(),
                        bitmap.rowBytes(), true,
                        std::vector<gfx::PNGCodec::Comment>(),
                        png_representation);
  return true;
}

}  // namespace browser
