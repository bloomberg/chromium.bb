// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtk2ui/gtk2_util.h"

#include <gtk/gtk.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/size.h"

namespace {

void CommonInitFromCommandLine(const CommandLine& command_line,
                               void (*init_func)(gint*, gchar***)) {
  const std::vector<std::string>& args = command_line.argv();
  int argc = args.size();
  scoped_ptr<char *[]> argv(new char *[argc + 1]);
  for (size_t i = 0; i < args.size(); ++i) {
    // TODO(piman@google.com): can gtk_init modify argv? Just being safe
    // here.
    argv[i] = strdup(args[i].c_str());
  }
  argv[argc] = NULL;
  char **argv_pointer = argv.get();

  init_func(&argc, &argv_pointer);
  for (size_t i = 0; i < args.size(); ++i) {
    free(argv[i]);
  }
}

}  // namespace

namespace libgtk2ui {

void GtkInitFromCommandLine(const CommandLine& command_line) {
  CommonInitFromCommandLine(command_line, gtk_init);
}

const SkBitmap GdkPixbufToImageSkia(GdkPixbuf* pixbuf) {
  // TODO(erg): What do we do in the case where the pixbuf fails these dchecks?
  // I would prefer to use our gtk based canvas, but that would require
  // recompiling half of our skia extensions with gtk support, which we can't
  // do in this build.
  DCHECK_EQ(GDK_COLORSPACE_RGB, gdk_pixbuf_get_colorspace(pixbuf));

  int n_channels = gdk_pixbuf_get_n_channels(pixbuf);
  int w = gdk_pixbuf_get_width(pixbuf);
  int h = gdk_pixbuf_get_height(pixbuf);

  SkBitmap ret;
  ret.setConfig(SkBitmap::kARGB_8888_Config, w, h);
  ret.allocPixels();
  ret.eraseColor(0);

  uint32_t* skia_data = static_cast<uint32_t*>(ret.getAddr(0, 0));

  if (n_channels == 4) {
    int total_length = w * h;
    guchar* gdk_pixels = gdk_pixbuf_get_pixels(pixbuf);

    // Now here's the trick: we need to convert the gdk data (which is RGBA and
    // isn't premultiplied) to skia (which can be anything and premultiplied).
    for (int i = 0; i < total_length; ++i, gdk_pixels += 4) {
      const unsigned char& red = gdk_pixels[0];
      const unsigned char& green = gdk_pixels[1];
      const unsigned char& blue = gdk_pixels[2];
      const unsigned char& alpha = gdk_pixels[3];

      skia_data[i] = SkPreMultiplyARGB(alpha, red, green, blue);
    }
  } else if (n_channels == 3) {
    // Because GDK makes rowstrides word aligned, we need to do something a bit
    // more complex when a pixel isn't perfectly a word of memory.
    int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    guchar* gdk_pixels = gdk_pixbuf_get_pixels(pixbuf);
    for (int y = 0; y < h; ++y) {
      int row = y * rowstride;

      for (int x = 0; x < w; ++x) {
        guchar* pixel = gdk_pixels + row + (x * 3);
        const unsigned char& red = pixel[0];
        const unsigned char& green = pixel[1];
        const unsigned char& blue = pixel[2];

        skia_data[y * w + x] = SkPreMultiplyARGB(255, red, green, blue);
      }
    }
  } else {
    NOTREACHED();
  }

  return ret;
}

}  // namespace libgtk2ui
