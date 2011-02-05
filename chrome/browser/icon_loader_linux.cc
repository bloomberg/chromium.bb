// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_loader.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/mime_util.h"
#include "base/threading/thread.h"
#include "base/string_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/gtk_util.h"

static int SizeToInt(IconLoader::IconSize size) {
  int pixels = 48;
  if (size == IconLoader::NORMAL)
    pixels = 32;
  else if (size == IconLoader::SMALL)
    pixels = 16;

  return pixels;
}

void IconLoader::ReadIcon() {
  filename_ = mime_util::GetMimeIcon(group_, SizeToInt(icon_size_));
  file_util::ReadFileToString(filename_, &icon_data_);
  target_message_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &IconLoader::ParseIcon));
}

void IconLoader::ParseIcon() {
  int size = SizeToInt(icon_size_);

  // It would be more convenient to use gdk_pixbuf_new_from_stream_at_scale
  // but that is only available after 2.14.
  GdkPixbufLoader* loader = gdk_pixbuf_loader_new();
  gdk_pixbuf_loader_set_size(loader, size, size);
  gdk_pixbuf_loader_write(loader,
                          reinterpret_cast<const guchar*>(icon_data_.data()),
                          icon_data_.length(), NULL);
  gdk_pixbuf_loader_close(loader, NULL);
  // We don't own a reference, we rely on the loader's ref.
  GdkPixbuf* pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);

  if (pixbuf) {
    guchar* pixels = gdk_pixbuf_get_pixels(pixbuf);
    int width = gdk_pixbuf_get_width(pixbuf);
    int height = gdk_pixbuf_get_height(pixbuf);
    DCHECK_EQ(width, size);
    DCHECK_EQ(height, size);
    int stride = gdk_pixbuf_get_rowstride(pixbuf);

    if (!gdk_pixbuf_get_has_alpha(pixbuf)) {
      LOG(WARNING) << "Got an image with no alpha channel, aborting load.";
    } else {
      uint8_t* BGRA_pixels =
          gfx::BGRAToRGBA(pixels, width, height, stride);
      std::vector<unsigned char> pixel_vector;
      pixel_vector.resize(height * stride);
      memcpy(const_cast<unsigned char*>(pixel_vector.data()), BGRA_pixels,
             height * stride);
      bitmap_.Set(gfx::PNGCodec::CreateSkBitmapFromBGRAFormat(pixel_vector,
                                                              width, height));
      free(BGRA_pixels);
    }
  } else {
    LOG(WARNING) << "Unsupported file type or load error: " <<
                    filename_.value();
  }

  g_object_unref(loader);

  NotifyDelegate();
}
