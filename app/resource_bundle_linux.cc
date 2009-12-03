// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/resource_bundle.h"

#include <gtk/gtk.h>

#include "app/app_paths.h"
#include "app/gfx/font.h"
#include "app/gfx/gtk_util.h"
#include "app/l10n_util.h"
#include "base/base_paths.h"
#include "base/data_pack.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {

// Convert the raw image data into a GdkPixbuf.  The GdkPixbuf that is returned
// has a ref count of 1 so the caller must call g_object_unref to free the
// memory.
GdkPixbuf* LoadPixbuf(RefCountedStaticMemory* data, bool rtl_enabled) {
  ScopedGObject<GdkPixbufLoader>::Type loader(gdk_pixbuf_loader_new());
  bool ok = gdk_pixbuf_loader_write(loader.get(),
      reinterpret_cast<const guint8*>(data->front()), data->size(), NULL);
  if (!ok)
    return NULL;
  // Calling gdk_pixbuf_loader_close forces the data to be parsed by the
  // loader.  We must do this before calling gdk_pixbuf_loader_get_pixbuf.
  ok = gdk_pixbuf_loader_close(loader.get(), NULL);
  if (!ok)
    return NULL;
  GdkPixbuf* pixbuf = gdk_pixbuf_loader_get_pixbuf(loader.get());
  if (!pixbuf)
    return NULL;

  if ((l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) &&
      rtl_enabled) {
    // |pixbuf| will get unreffed and destroyed (see below). The returned value
    // has ref count 1.
    return gdk_pixbuf_flip(pixbuf, TRUE);
  } else {
    // The pixbuf is owned by the loader, so add a ref so when we delete the
    // loader (when the ScopedGObject goes out of scope), the pixbuf still
    // exists.
    g_object_ref(pixbuf);
    return pixbuf;
  }
}

}  // namespace

void ResourceBundle::FreeGdkPixBufs() {
  for (GdkPixbufMap::iterator i = gdk_pixbufs_.begin();
       i != gdk_pixbufs_.end(); i++) {
    g_object_unref(i->second);
  }
  gdk_pixbufs_.clear();
}

// static
FilePath ResourceBundle::GetResourcesFilePath() {
  FilePath resources_file_path;
  PathService::Get(base::DIR_EXE, &resources_file_path);
  if (resources_file_path.empty())
    return resources_file_path;
  return resources_file_path.Append(FILE_PATH_LITERAL("chrome.pak"));
}

// static
FilePath ResourceBundle::GetLocaleFilePath(const std::wstring& pref_locale) {
  FilePath locale_file_path;
  PathService::Get(app::DIR_LOCALES, &locale_file_path);
  if (locale_file_path.empty())
    return locale_file_path;
  const std::string app_locale = l10n_util::GetApplicationLocale(pref_locale);
  if (app_locale.empty())
    return FilePath();
  locale_file_path = locale_file_path.AppendASCII(app_locale + ".pak");
  if (!file_util::PathExists(locale_file_path))
    return FilePath();
  return locale_file_path;
}

GdkPixbuf* ResourceBundle::GetPixbufImpl(int resource_id, bool rtl_enabled) {
  // Use the negative |resource_id| for the key for BIDI-aware images.
  int key = rtl_enabled ? -resource_id : resource_id;

  // Check to see if we already have the pixbuf in the cache.
  {
    AutoLock lock_scope(lock_);
    GdkPixbufMap::const_iterator found = gdk_pixbufs_.find(key);
    if (found != gdk_pixbufs_.end())
      return found->second;
  }

  scoped_refptr<RefCountedStaticMemory> data(
      LoadDataResourceBytes(resource_id));
  GdkPixbuf* pixbuf = LoadPixbuf(data.get(), rtl_enabled);

  // We loaded successfully.  Cache the pixbuf.
  if (pixbuf) {
    AutoLock lock_scope(lock_);

    // Another thread raced us, and has already cached the pixbuf.
    if (gdk_pixbufs_.count(key)) {
      g_object_unref(pixbuf);
      return gdk_pixbufs_[key];
    }

    gdk_pixbufs_[key] = pixbuf;
    return pixbuf;
  }

  // We failed to retrieve the bitmap, show a debugging red square.
  {
    LOG(WARNING) << "Unable to load GdkPixbuf with id " << resource_id;
    NOTREACHED();  // Want to assert in debug mode.

    AutoLock lock_scope(lock_);  // Guard empty_bitmap initialization.

    static GdkPixbuf* empty_bitmap = NULL;
    if (!empty_bitmap) {
      // The placeholder bitmap is bright red so people notice the problem.
      // This bitmap will be leaked, but this code should never be hit.
      scoped_ptr<SkBitmap> skia_bitmap(new SkBitmap());
      skia_bitmap->setConfig(SkBitmap::kARGB_8888_Config, 32, 32);
      skia_bitmap->allocPixels();
      skia_bitmap->eraseARGB(255, 255, 0, 0);
      empty_bitmap = gfx::GdkPixbufFromSkBitmap(skia_bitmap.get());
    }
    return empty_bitmap;
  }
}

GdkPixbuf* ResourceBundle::GetPixbufNamed(int resource_id) {
  return GetPixbufImpl(resource_id, false);
}

GdkPixbuf* ResourceBundle::GetRTLEnabledPixbufNamed(int resource_id) {
  return GetPixbufImpl(resource_id, true);
}
