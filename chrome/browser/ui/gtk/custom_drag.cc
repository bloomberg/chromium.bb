// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/custom_drag.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/download/download_item.h"
#include "chrome/browser/ui/gtk/bookmarks/bookmark_utils_gtk.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/dragdrop/gtk_dnd_util.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/image.h"

namespace {

const int kDownloadItemCodeMask = ui::TEXT_URI_LIST | ui::CHROME_NAMED_URL;
const GdkDragAction kDownloadItemDragAction = GDK_ACTION_COPY;
const GdkDragAction kBookmarkDragAction =
    static_cast<GdkDragAction>(GDK_ACTION_COPY | GDK_ACTION_MOVE);

void OnDragDataGetForDownloadItem(GtkSelectionData* selection_data,
                                  guint target_type,
                                  const DownloadItem* download_item) {
  GURL url = net::FilePathToFileURL(download_item->full_path());
  ui::WriteURLWithName(selection_data, url,
      UTF8ToUTF16(download_item->GetFileNameToReportUser().value()),
      target_type);
}

void OnDragDataGetStandalone(GtkWidget* widget, GdkDragContext* context,
                             GtkSelectionData* selection_data,
                             guint target_type, guint time,
                             const DownloadItem* item) {
  OnDragDataGetForDownloadItem(selection_data, target_type, item);
}

}  // namespace

// CustomDrag ------------------------------------------------------------------

CustomDrag::CustomDrag(gfx::Image* icon, int code_mask, GdkDragAction action)
    : drag_widget_(gtk_invisible_new()),
      image_(icon) {
  g_signal_connect(drag_widget_, "drag-data-get",
                   G_CALLBACK(OnDragDataGetThunk), this);
  g_signal_connect(drag_widget_, "drag-begin",
                   G_CALLBACK(OnDragBeginThunk), this);
  g_signal_connect(drag_widget_, "drag-end",
                   G_CALLBACK(OnDragEndThunk), this);

  GtkTargetList* list = ui::GetTargetListFromCodeMask(code_mask);
  GdkEvent* event = gtk_get_current_event();
  gtk_drag_begin(drag_widget_, list, action, 1, event);
  if (event)
    gdk_event_free(event);
  gtk_target_list_unref(list);
}

CustomDrag::~CustomDrag() {
  gtk_widget_destroy(drag_widget_);
}

void CustomDrag::OnDragBegin(GtkWidget* widget, GdkDragContext* drag_context) {
  if (image_)
    gtk_drag_set_icon_pixbuf(drag_context, *image_, 0, 0);
}

void CustomDrag::OnDragEnd(GtkWidget* widget, GdkDragContext* drag_context) {
  delete this;
}

// DownloadItemDrag ------------------------------------------------------------

DownloadItemDrag::DownloadItemDrag(const DownloadItem* item,
                                   gfx::Image* icon)
    : CustomDrag(icon, kDownloadItemCodeMask, kDownloadItemDragAction),
      download_item_(item) {
}

DownloadItemDrag::~DownloadItemDrag() {
}

void DownloadItemDrag::OnDragDataGet(
    GtkWidget* widget, GdkDragContext* context,
    GtkSelectionData* selection_data,
    guint target_type, guint time) {
  OnDragDataGetForDownloadItem(selection_data, target_type, download_item_);
}

// static
void DownloadItemDrag::SetSource(GtkWidget* widget,
                                 DownloadItem* item,
                                 gfx::Image* icon) {
  gtk_drag_source_set(widget, GDK_BUTTON1_MASK, NULL, 0,
                      kDownloadItemDragAction);
  ui::SetSourceTargetListFromCodeMask(widget, kDownloadItemCodeMask);

  // Disconnect previous signal handlers, if any.
  g_signal_handlers_disconnect_by_func(
      widget,
      reinterpret_cast<gpointer>(OnDragDataGetStandalone),
      item);
  // Connect new signal handlers.
  g_signal_connect(widget, "drag-data-get",
                   G_CALLBACK(OnDragDataGetStandalone), item);

  if (icon)
    gtk_drag_source_set_icon_pixbuf(widget, *icon);
}

// static
void DownloadItemDrag::BeginDrag(const DownloadItem* item, gfx::Image* icon) {
  new DownloadItemDrag(item, icon);
}

// BookmarkDrag ----------------------------------------------------------------

BookmarkDrag::BookmarkDrag(Profile* profile,
                           const std::vector<const BookmarkNode*>& nodes)
    : CustomDrag(NULL,
                 bookmark_utils::GetCodeMask(false),
                 kBookmarkDragAction),
      profile_(profile),
      nodes_(nodes) {
}

BookmarkDrag::~BookmarkDrag() {
}

void BookmarkDrag::OnDragDataGet(GtkWidget* widget, GdkDragContext* context,
                                 GtkSelectionData* selection_data,
                                 guint target_type, guint time) {
  bookmark_utils::WriteBookmarksToSelection(nodes_, selection_data,
                                            target_type, profile_);
}

// static
void BookmarkDrag::BeginDrag(Profile* profile,
                             const std::vector<const BookmarkNode*>& nodes) {
  new BookmarkDrag(profile, nodes);
}
