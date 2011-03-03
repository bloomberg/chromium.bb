// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
  // At this point, the pixbuf is owned by the loader.
  GdkPixbuf* pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);

  if (pixbuf) {
    DCHECK_EQ(size, gdk_pixbuf_get_width(pixbuf));
    DCHECK_EQ(size, gdk_pixbuf_get_height(pixbuf));
    // Takes ownership of |pixbuf|.
    g_object_ref(pixbuf);
    image_.reset(new gfx::Image(pixbuf));
  } else {
    LOG(WARNING) << "Unsupported file type or load error: " <<
                    filename_.value();
  }

  g_object_unref(loader);

  NotifyDelegate();
}
