// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/download/download_shelf_context_menu_gtk.h"

#include "base/logging.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/ui/gtk/download/download_item_gtk.h"
#include "content/public/browser/page_navigator.h"
#include "ui/gfx/point.h"

DownloadShelfContextMenuGtk::DownloadShelfContextMenuGtk(
    DownloadItemGtk* download_item,
    content::PageNavigator* navigator)
    : DownloadShelfContextMenu(download_item->download(), navigator),
      download_item_gtk_(download_item) {
}

DownloadShelfContextMenuGtk::~DownloadShelfContextMenuGtk() {}

void DownloadShelfContextMenuGtk::Popup(GtkWidget* widget,
                                        GdkEventButton* event) {
  ui::SimpleMenuModel* menu_model = GetMenuModel();
  // Popup() should never be called after the DownloadItem is destroyed.
  DCHECK(menu_model);

  menu_.reset(new MenuGtk(this, menu_model));

  if (widget)
    menu_->PopupForWidget(widget, event->button, event->time);
  else
    menu_->PopupAsContext(gfx::Point(event->x_root, event->y_root),
                          event->time);
}

void DownloadShelfContextMenuGtk::StoppedShowing() {
  download_item_gtk_->menu_showing_ = false;
  gtk_widget_queue_draw(download_item_gtk_->menu_button_);
}

GtkWidget* DownloadShelfContextMenuGtk::GetImageForCommandId(
    int command_id) const {
  const char* stock = NULL;
  switch (command_id) {
    case SHOW_IN_FOLDER:
    case OPEN_WHEN_COMPLETE:
      stock = GTK_STOCK_OPEN;
      break;

    case CANCEL:
      stock = GTK_STOCK_CANCEL;
      break;

    case ALWAYS_OPEN_TYPE:
    case TOGGLE_PAUSE:
      stock = NULL;
      break;

    default:
      NOTREACHED();
      break;
  }
  return stock ? gtk_image_new_from_stock(stock, GTK_ICON_SIZE_MENU) : NULL;
}
