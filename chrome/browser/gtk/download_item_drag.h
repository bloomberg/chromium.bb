// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_DOWNLOAD_ITEM_DRAG_H_
#define CHROME_BROWSER_GTK_DOWNLOAD_ITEM_DRAG_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"

class DownloadItem;
class SkBitmap;

class DownloadItemDrag {
 public:
  // Sets |widget| as a source for drags pertaining to |item|. No
  // DownloadItemDrag object is created.
  static void SetSource(GtkWidget* widget, DownloadItem* item);

  // Creates a new DownloadItemDrag, the lifetime of which is tied to the
  // system drag.
  static void BeginDrag(const DownloadItem* item, SkBitmap* icon);

 private:
  explicit DownloadItemDrag(const DownloadItem* item, SkBitmap* icon);
  ~DownloadItemDrag();

  static void OnDragBegin(GtkWidget* widget,
                          GdkDragContext* drag_context,
                          DownloadItemDrag* drag);

  static void OnDragEnd(GtkWidget* widget,
                        GdkDragContext* drag_context,
                        DownloadItemDrag* drag);

  GtkWidget* drag_widget_;
  GdkPixbuf* pixbuf_;

  DISALLOW_COPY_AND_ASSIGN(DownloadItemDrag);
};

#endif  // CHROME_BROWSER_GTK_DOWNLOAD_ITEM_DRAG_H_
