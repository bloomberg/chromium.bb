// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/status_icons/status_icon_gtk.h"

#include "base/string16.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/gtk/menu_gtk.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/gtk_util.h"

StatusIconGtk::StatusIconGtk() {
  icon_ = gtk_status_icon_new();
  gtk_status_icon_set_visible(icon_, TRUE);

  g_signal_connect(icon_, "activate",
                   G_CALLBACK(OnClickThunk), this);
  g_signal_connect(icon_, "popup-menu",
                   G_CALLBACK(OnPopupMenuThunk), this);
}

StatusIconGtk::~StatusIconGtk() {
  g_object_unref(icon_);
}

void StatusIconGtk::SetImage(const SkBitmap& image) {
  if (image.isNull())
    return;

  GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(&image);
  gtk_status_icon_set_from_pixbuf(icon_, pixbuf);
  g_object_unref(pixbuf);
}

void StatusIconGtk::SetPressedImage(const SkBitmap& image) {
  // Ignore pressed images, since the standard on Linux is to not highlight
  // pressed status icons.
}

void StatusIconGtk::SetToolTip(const string16& tool_tip) {
  gtk_status_icon_set_tooltip(icon_, UTF16ToUTF8(tool_tip).c_str());
}

void StatusIconGtk::OnClick(GtkWidget* widget) {
  DispatchClickEvent();
}

void StatusIconGtk::UpdatePlatformContextMenu(ui::MenuModel* model) {
  if (!model)
    menu_.reset();
  else
    menu_.reset(new MenuGtk(NULL, model));
}

void StatusIconGtk::OnPopupMenu(GtkWidget* widget, guint button, guint time) {
  // If we have a menu - display it.
  if (menu_.get())
    menu_->PopupAsContextForStatusIcon(time, button, icon_);
}
