// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/window_icon_util.h"

#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include "ui/gfx/x/x11_types.h"

gfx::ImageSkia GetWindowIcon(content::DesktopMediaID id) {
  DCHECK(id.type == content::DesktopMediaID::TYPE_WINDOW);

  Display* display = gfx::GetXDisplay();
  Atom property = XInternAtom(display, "_NET_WM_ICON", True);
  Atom actual_type;
  int actual_format;
  unsigned long bytes_after;  // NOLINT: type required by XGetWindowProperty
  unsigned long size;
  long* data;

  int status = XGetWindowProperty(display, id.id, property, 0L, ~0L, False,
                                  AnyPropertyType, &actual_type, &actual_format,
                                  &size, &bytes_after,
                                  reinterpret_cast<unsigned char**>(&data));
  if (status != Success) {
    return gfx::ImageSkia();
  }

  // The format of |data| is concatenation of sections like
  // [width, height, pixel data of size width * height], and the total bytes
  // number of |data| is |size|. And here we are picking the largest icon.
  int width = 0;
  int height = 0;
  int start = 0;
  int i = 0;
  while (i + 1 < static_cast<int>(size)) {
    if ((i == 0 || static_cast<int>(data[i] * data[i + 1]) > width * height) &&
        (i + 1 + data[i] * data[i + 1] < static_cast<int>(size))) {
      width = static_cast<int>(data[i]);
      height = static_cast<int>(data[i + 1]);
      start = i + 2;
    }
    i = i + 2 + static_cast<int>(data[i] * data[i + 1]);
  }

  SkBitmap result;
  SkImageInfo info = SkImageInfo::MakeN32(width, height, kUnpremul_SkAlphaType);
  result.allocPixels(info);
  result.lockPixels();

  uint32_t* pixels_data = reinterpret_cast<uint32_t*>(result.getPixels());

  for (long y = 0; y < height; ++y) {
    for (long x = 0; x < width; ++x) {
      pixels_data[result.rowBytesAsPixels() * y + x] =
          static_cast<uint32_t>(data[start + width * y + x]);
    }
  }

  XFree(data);
  return gfx::ImageSkia::CreateFrom1xBitmap(result);
}
