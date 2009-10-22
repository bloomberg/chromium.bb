// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/list_store_favicon_loader.h"

#include <vector>

#include "app/gfx/codec/png_codec.h"
#include "app/gfx/gtk_util.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/profile.h"
#include "third_party/skia/include/core/SkBitmap.h"

ListStoreFavIconLoader::ListStoreFavIconLoader(
    GtkListStore* list_store, gint favicon_col, gint favicon_handle_col,
    Profile* profile, CancelableRequestConsumer* consumer)
    : list_store_(list_store), favicon_col_(favicon_col),
      favicon_handle_col_(favicon_handle_col), profile_(profile),
      consumer_(consumer),
      default_favicon_(GtkThemeProvider::GetDefaultFavicon(true)) {
}

ListStoreFavIconLoader::~ListStoreFavIconLoader() {
}

void ListStoreFavIconLoader::LoadFaviconForRow(const GURL& url,
                                               GtkTreeIter* iter) {
  FaviconService* favicon_service =
      profile_->GetFaviconService(Profile::EXPLICIT_ACCESS);
  if (favicon_service) {
    FaviconService::Handle handle = favicon_service->GetFaviconForURL(
        url, consumer_,
        NewCallback(this, &ListStoreFavIconLoader::OnGotFavIcon));
    gtk_list_store_set(list_store_, iter,
                       favicon_handle_col_, handle,
                       favicon_col_, default_favicon_,
                       -1);
  }
}

bool ListStoreFavIconLoader::GetRowByFavIconHandle(
    FaviconService::Handle handle, GtkTreeIter* result_iter) {
  GtkTreeIter iter;
  gboolean valid = gtk_tree_model_get_iter_first(
      GTK_TREE_MODEL(list_store_), &iter);
  while (valid) {
    gint row_handle;
    gtk_tree_model_get(GTK_TREE_MODEL(list_store_), &iter,
                       favicon_handle_col_, &row_handle,
                       -1);
    if (row_handle == handle) {
      *result_iter = iter;
      return true;
    }
    valid = gtk_tree_model_iter_next(
        GTK_TREE_MODEL(list_store_), &iter);
  }
  return false;
}

void ListStoreFavIconLoader::OnGotFavIcon(
    FaviconService::Handle handle, bool know_fav_icon,
    scoped_refptr<RefCountedBytes> image_data, bool is_expired, GURL icon_url) {
  GtkTreeIter iter;
  if (!GetRowByFavIconHandle(handle, &iter))
    return;
  gtk_list_store_set(list_store_, &iter,
                     favicon_handle_col_, 0,
                     -1);
  if (know_fav_icon && image_data.get() && !image_data->data.empty()) {
    SkBitmap icon;
    if (gfx::PNGCodec::Decode(&image_data->data.front(),
                              image_data->data.size(), &icon)) {
      GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(&icon);
      gtk_list_store_set(list_store_, &iter,
                         favicon_col_, pixbuf,
                         -1);
      g_object_unref(pixbuf);
    }
  }
}
