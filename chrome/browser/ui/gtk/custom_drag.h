// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_CUSTOM_DRAG_H_
#define CHROME_BROWSER_UI_GTK_CUSTOM_DRAG_H_
#pragma once

#include <gtk/gtk.h>
#include <vector>

#include "base/basictypes.h"
#include "ui/base/gtk/gtk_signal.h"

class BookmarkNode;
class DownloadItem;
class Profile;
class SkBitmap;

// Base class for programatically generated drags.
class CustomDrag {
 protected:
  explicit CustomDrag(SkBitmap* icon, int code_mask, GdkDragAction action);
  virtual ~CustomDrag();

  virtual void OnDragDataGet(GtkWidget* widget, GdkDragContext* context,
                             GtkSelectionData* selection_data,
                             guint target_type, guint time) = 0;

 private:
  CHROMEGTK_CALLBACK_1(CustomDrag, void, OnDragBegin, GdkDragContext*);
  CHROMEGTK_CALLBACK_1(CustomDrag, void, OnDragEnd, GdkDragContext*);

  // Since this uses a virtual function, we can't use a macro.
  static void OnDragDataGetThunk(GtkWidget* widget, GdkDragContext* context,
                                 GtkSelectionData* selection_data,
                                 guint target_type, guint time,
                                 CustomDrag* custom_drag) {
    return custom_drag->OnDragDataGet(widget, context, selection_data,
                                      target_type, time);
  }

  // Can't use a OwnedWidgetGtk because the initialization of GtkInvisible
  // sinks the reference.
  GtkWidget* drag_widget_;

  GdkPixbuf* pixbuf_;

  DISALLOW_COPY_AND_ASSIGN(CustomDrag);
};

// Encapsulates functionality for drags of download items.
class DownloadItemDrag : public CustomDrag {
 public:
  // Sets |widget| as a source for drags pertaining to |item|. No
  // DownloadItemDrag object is created.
  // It is safe to call this multiple times with different values of |icon|.
  static void SetSource(GtkWidget* widget, DownloadItem* item, SkBitmap* icon);

  // Creates a new DownloadItemDrag, the lifetime of which is tied to the
  // system drag.
  static void BeginDrag(const DownloadItem* item, SkBitmap* icon);

 private:
  DownloadItemDrag(const DownloadItem* item, SkBitmap* icon);
  virtual ~DownloadItemDrag();

  virtual void OnDragDataGet(GtkWidget* widget, GdkDragContext* context,
                             GtkSelectionData* selection_data,
                             guint target_type, guint time);

  const DownloadItem* download_item_;

  DISALLOW_COPY_AND_ASSIGN(DownloadItemDrag);
};

// Encapsulates functionality for drags of one or more bookmarks.
class BookmarkDrag : public CustomDrag {
 public:
  // Creates a new BookmarkDrag, the lifetime of which is tied to the
  // system drag.
  static void BeginDrag(Profile* profile,
                        const std::vector<const BookmarkNode*>& nodes);

 private:
  BookmarkDrag(Profile* profile,
               const std::vector<const BookmarkNode*>& nodes);
  virtual ~BookmarkDrag();

  virtual void OnDragDataGet(GtkWidget* widget, GdkDragContext* context,
                             GtkSelectionData* selection_data,
                             guint target_type, guint time);

  Profile* profile_;
  std::vector<const BookmarkNode*> nodes_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkDrag);
};

#endif  // CHROME_BROWSER_UI_GTK_CUSTOM_DRAG_H_
