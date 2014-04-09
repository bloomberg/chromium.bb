// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/download/download_item_drag.h"

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/download/drag_download_item.h"
#include "content/public/browser/download_item.h"
#include "net/base/filename_util.h"
#include "ui/base/dragdrop/gtk_dnd_util.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

using content::DownloadItem;

namespace {

const int kDownloadItemCodeMask = ui::TEXT_URI_LIST | ui::CHROME_NAMED_URL;
const GdkDragAction kDownloadItemDragAction = GDK_ACTION_COPY;

}  // namespace

// Stores metadata for a drag & drop operation.
class DownloadItemDrag::DragData {
 public:
  // Constructs a DragData object based on the current state of |item|.
  explicit DragData(const DownloadItem* item);

  // Sets up a drag source and connects |drag_data| to 'drag-data-get' on
  // |widget|. If |icon| is non-NULL it will be used as the drag icon. The
  // object pointed to by |drag_data| will be deleted when the signal is
  // disconnected.
  static void AttachToWidget(scoped_ptr<DragData> drag_data,
                             GtkWidget* widget,
                             gfx::Image* icon);

  // 'drag-data-get' signal handler.
  CHROMEGTK_CALLBACK_4(DragData, void, OnDragDataGet, GdkDragContext*,
                       GtkSelectionData*, guint, guint);

 private:
  // GClosureNotify handler for destroying a DragData object. |data| is assumed
  // to be a DragData*.
  static void OnDestroy(gpointer data, GClosure* closure);

  GURL url_;
  base::string16 display_name_;
};

DownloadItemDrag::DragData::DragData(const DownloadItem* item)
    : url_(net::FilePathToFileURL(item->GetTargetFilePath())),
      display_name_(item->GetFileNameToReportUser().LossyDisplayName()) {
  DCHECK_EQ(DownloadItem::COMPLETE, item->GetState());
}

// static
void DownloadItemDrag::DragData::AttachToWidget(scoped_ptr<DragData> drag_data,
                                                GtkWidget* widget,
                                                gfx::Image* icon) {
  gtk_drag_source_set(
      widget, GDK_BUTTON1_MASK, NULL, 0, kDownloadItemDragAction);
  ui::SetSourceTargetListFromCodeMask(widget, kDownloadItemCodeMask);

  // Disconnect previous signal handlers, if any.
  g_signal_handlers_disconnect_matched(
      widget,
      G_SIGNAL_MATCH_FUNC,
      0,
      0,
      NULL,
      reinterpret_cast<gpointer>(&OnDragDataGetThunk),
      NULL);

  // Connect new signal handlers.
  g_signal_connect_data(widget,
                        "drag-data-get",
                        G_CALLBACK(&OnDragDataGetThunk),
                        reinterpret_cast<gpointer>(drag_data.release()),
                        &OnDestroy,
                        static_cast<GConnectFlags>(0));

  if (icon)
    gtk_drag_source_set_icon_pixbuf(widget, icon->ToGdkPixbuf());
}

void DownloadItemDrag::DragData::OnDragDataGet(GtkWidget* widget,
                                               GdkDragContext* context,
                                               GtkSelectionData* selection_data,
                                               guint target_type,
                                               guint time) {
  ui::WriteURLWithName(selection_data, url_, display_name_, target_type);
}

// static
void DownloadItemDrag::DragData::OnDestroy(gpointer data, GClosure* closure) {
  DragData* drag_data = reinterpret_cast<DragData*>(data);
  delete drag_data;
}

// DownloadItemDrag ------------------------------------------------------------

// static
void DownloadItemDrag::SetSource(GtkWidget* widget,
                                 const DownloadItem* item,
                                 gfx::Image* icon) {
  scoped_ptr<DragData> drag_data(new DragData(item));
  DragData::AttachToWidget(drag_data.Pass(), widget, icon);
}

DownloadItemDrag::DownloadItemDrag(const DownloadItem* item, gfx::Image* icon)
    : CustomDrag(icon, kDownloadItemCodeMask, kDownloadItemDragAction),
      drag_data_(new DragData(item)) {}

DownloadItemDrag::~DownloadItemDrag() {}

void DownloadItemDrag::OnDragDataGet(GtkWidget* widget,
                                     GdkDragContext* context,
                                     GtkSelectionData* selection_data,
                                     guint target_type,
                                     guint time) {
  drag_data_->OnDragDataGet(widget, context, selection_data, target_type, time);
}

void DragDownloadItem(const content::DownloadItem* download,
                      gfx::Image* icon,
                      gfx::NativeView view) {
  // This starts the drag process, the lifetime of this object is tied to the
  // system drag.
  new DownloadItemDrag(download, icon);
}
