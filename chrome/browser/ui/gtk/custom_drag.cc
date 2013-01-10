// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/custom_drag.h"

#include "base/file_path.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/gtk/bookmarks/bookmark_utils_gtk.h"
#include "content/public/browser/download_item.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/dragdrop/gtk_dnd_util.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/image/image.h"

using content::DownloadItem;

namespace {

const int kDownloadItemCodeMask = ui::TEXT_URI_LIST | ui::CHROME_NAMED_URL;
const GdkDragAction kDownloadItemDragAction = GDK_ACTION_COPY;
const GdkDragAction kBookmarkDragAction =
    static_cast<GdkDragAction>(GDK_ACTION_COPY | GDK_ACTION_MOVE);

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
    gtk_drag_set_icon_pixbuf(drag_context, image_->ToGdkPixbuf(), 0, 0);
}

void CustomDrag::OnDragEnd(GtkWidget* widget, GdkDragContext* drag_context) {
  delete this;
}

// DownloadItemDrag ------------------------------------------------------------

// Stores metadata for a drag & drop operation.
class DownloadItemDrag::DragData {
 public:
  // Constructs a DragData object based on the current state of |item|.
  explicit DragData(const DownloadItem* item);

  // 'drag-data-get' signal handler.
  CHROMEGTK_CALLBACK_4(DragData, void, OnDragDataGet, GdkDragContext*,
                       GtkSelectionData*, guint, guint);

  // Sets up a drag source and connects |drag_data| to 'drag-data-get' on
  // |widget|. If |icon| is non-NULL it will be used as the drag icon. The
  // object pointed to by |drag_data| will be deleted when the signal is
  // disconnected.
  static void AttachToWidget(scoped_ptr<DragData> drag_data,
                             GtkWidget* widget,
                             gfx::Image* icon);

 private:
  // GClosureNotify handler for destroying a DragData object. |data| is assumed
  // to be a DragData*.
  static void OnDestroy(gpointer data, GClosure* closure);

  GURL url_;
  string16 display_name_;
};

DownloadItemDrag::DragData::DragData(const DownloadItem* item)
    : url_(net::FilePathToFileURL(item->GetTargetFilePath())),
      display_name_(item->GetFileNameToReportUser().LossyDisplayName()) {
  DCHECK(item->IsComplete());
}

void DownloadItemDrag::DragData::OnDragDataGet(GtkWidget* widget,
                                               GdkDragContext* context,
                                               GtkSelectionData* selection_data,
                                               guint target_type,
                                               guint time) {
  ui::WriteURLWithName(selection_data, url_, display_name_, target_type);
}

// static
void DownloadItemDrag::DragData::AttachToWidget(scoped_ptr<DragData> drag_data,
                                                GtkWidget* widget,
                                                gfx::Image* icon) {
  gtk_drag_source_set(widget, GDK_BUTTON1_MASK, NULL, 0,
                      kDownloadItemDragAction);
  ui::SetSourceTargetListFromCodeMask(widget, kDownloadItemCodeMask);

  // Disconnect previous signal handlers, if any.
  g_signal_handlers_disconnect_matched(
      widget, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
      reinterpret_cast<gpointer>(&OnDragDataGetThunk),
      NULL);

  // Connect new signal handlers.
  g_signal_connect_data(
      widget, "drag-data-get",
      G_CALLBACK(&OnDragDataGetThunk),
      reinterpret_cast<gpointer>(drag_data.release()),
      &OnDestroy,
      static_cast<GConnectFlags>(0));

  if (icon)
    gtk_drag_source_set_icon_pixbuf(widget, icon->ToGdkPixbuf());
}

// static
void DownloadItemDrag::DragData::OnDestroy(gpointer data, GClosure* closure) {
  DragData* drag_data = reinterpret_cast<DragData*>(data);
  delete drag_data;
}

DownloadItemDrag::DownloadItemDrag(const DownloadItem* item,
                                   gfx::Image* icon)
    : CustomDrag(icon, kDownloadItemCodeMask, kDownloadItemDragAction),
      drag_data_(new DragData(item)) {
}

DownloadItemDrag::~DownloadItemDrag() {
}

void DownloadItemDrag::OnDragDataGet(
    GtkWidget* widget, GdkDragContext* context,
    GtkSelectionData* selection_data,
    guint target_type, guint time) {
  drag_data_->OnDragDataGet(widget, context, selection_data, target_type, time);
}

// static
void DownloadItemDrag::SetSource(GtkWidget* widget,
                                 const DownloadItem* item,
                                 gfx::Image* icon) {
  scoped_ptr<DragData> drag_data(new DragData(item));
  DragData::AttachToWidget(drag_data.Pass(), widget, icon);
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
