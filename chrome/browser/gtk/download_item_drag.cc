// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/download_item_drag.h"

#include "app/gfx/gtk_util.h"
#include "app/gtk_dnd_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/download/download_manager.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {

const int kCodeMask = GtkDndUtil::TEXT_URI_LIST |
                      GtkDndUtil::CHROME_NAMED_URL;
const GdkDragAction kDragAction = GDK_ACTION_COPY;

void OnDragDataGet(GtkWidget* widget, GdkDragContext* context,
                   GtkSelectionData* selection_data,
                   guint target_type, guint time,
                   DownloadItem* download_item) {
  GURL url = net::FilePathToFileURL(download_item->full_path());
  GtkDndUtil::WriteURLWithName(selection_data, url,
      UTF8ToUTF16(download_item->GetFileName().value()), target_type);
}

}  // namespace

// static
void DownloadItemDrag::SetSource(GtkWidget* widget, DownloadItem* item) {
  gtk_drag_source_set(widget, GDK_BUTTON1_MASK, NULL, 0,
                      kDragAction);
  GtkDndUtil::SetSourceTargetListFromCodeMask(widget, kCodeMask);
  g_signal_connect(widget, "drag-data-get",
                   G_CALLBACK(OnDragDataGet), item);
}

// static
void DownloadItemDrag::BeginDrag(const DownloadItem* item, SkBitmap* icon) {
  new DownloadItemDrag(item, icon);
}

DownloadItemDrag::DownloadItemDrag(const DownloadItem* item,
                                   SkBitmap* icon)
    : drag_widget_(gtk_invisible_new()),
      pixbuf_(gfx::GdkPixbufFromSkBitmap(icon)) {
  g_object_ref_sink(drag_widget_);
  g_signal_connect(drag_widget_, "drag-data-get",
                   G_CALLBACK(OnDragDataGet), const_cast<DownloadItem*>(item));
  g_signal_connect(drag_widget_, "drag-begin",
                   G_CALLBACK(OnDragBegin), this);
  g_signal_connect(drag_widget_, "drag-end",
                   G_CALLBACK(OnDragEnd), this);

  GtkTargetList* list = GtkDndUtil::GetTargetListFromCodeMask(kCodeMask);
  GdkEvent* event = gtk_get_current_event();
  gtk_drag_begin(drag_widget_, list, kDragAction, 1, event);
  if (event)
    gdk_event_free(event);
  gtk_target_list_unref(list);
}

DownloadItemDrag::~DownloadItemDrag() {
  g_object_unref(pixbuf_);
  g_object_unref(drag_widget_);
}

// static
void DownloadItemDrag::OnDragBegin(GtkWidget* widget,
                                   GdkDragContext* drag_context,
                                   DownloadItemDrag* drag) {
  gtk_drag_set_icon_pixbuf(drag_context, drag->pixbuf_, 0, 0);
}

// static
void DownloadItemDrag::OnDragEnd(GtkWidget* widget,
                                 GdkDragContext* drag_context,
                                 DownloadItemDrag* drag) {
  delete drag;
}
