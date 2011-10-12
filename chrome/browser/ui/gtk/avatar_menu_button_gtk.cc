// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/avatar_menu_button_gtk.h"

#include "base/i18n/rtl.h"
#include "chrome/browser/ui/gtk/avatar_menu_bubble_gtk.h"
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"
#include "ui/gfx/gtk_util.h"

AvatarMenuButtonGtk::AvatarMenuButtonGtk(Browser* browser)
    : image_(NULL),
      browser_(browser),
      arrow_location_(BubbleGtk::ARROW_LOCATION_TOP_LEFT) {
  GtkWidget* event_box = gtk_event_box_new();
  image_ = gtk_image_new();
  gtk_container_add(GTK_CONTAINER(event_box), image_);

  g_signal_connect(event_box, "button-press-event",
                   G_CALLBACK(OnButtonPressedThunk), this);

  gtk_event_box_set_visible_window(GTK_EVENT_BOX(event_box), FALSE);

  widget_.Own(event_box);
}

AvatarMenuButtonGtk::~AvatarMenuButtonGtk() {
}

void AvatarMenuButtonGtk::SetIcon(const SkBitmap& icon) {
  GdkPixbuf* icon_pixbuf = gfx::GdkPixbufFromSkBitmap(&icon);
  gtk_image_set_from_pixbuf(GTK_IMAGE(image_), icon_pixbuf);
  g_object_unref(icon_pixbuf);

  gtk_misc_set_alignment(GTK_MISC(image_), 0.0, 1.0);
  gtk_widget_set_size_request(image_, -1, 0);
}

gboolean AvatarMenuButtonGtk::OnButtonPressed(GtkWidget* widget,
                                              GdkEventButton* event) {
  if (event->button != 1)
    return FALSE;

  new AvatarMenuBubbleGtk(browser_, widget, arrow_location_);
  return TRUE;
}
