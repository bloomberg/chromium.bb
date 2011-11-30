// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/avatar_menu_button_gtk.h"

#include "base/i18n/rtl.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_info_util.h"
#include "chrome/browser/ui/gtk/avatar_menu_bubble_gtk.h"
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"
#include "ui/gfx/gtk_util.h"

AvatarMenuButtonGtk::AvatarMenuButtonGtk(Browser* browser)
    : image_(NULL),
      browser_(browser),
      arrow_location_(BubbleGtk::ARROW_LOCATION_TOP_LEFT),
      is_gaia_picture_(false),
      old_height_(0) {
  GtkWidget* event_box = gtk_event_box_new();
  image_ = gtk_image_new();
  gtk_container_add(GTK_CONTAINER(event_box), image_);

  g_signal_connect(event_box, "button-press-event",
                   G_CALLBACK(OnButtonPressedThunk), this);
  g_signal_connect(event_box, "size-allocate",
                   G_CALLBACK(OnSizeAllocateThunk), this);

  gtk_event_box_set_visible_window(GTK_EVENT_BOX(event_box), FALSE);

  widget_.Own(event_box);
}

AvatarMenuButtonGtk::~AvatarMenuButtonGtk() {
}

void AvatarMenuButtonGtk::SetIcon(const gfx::Image& image,
                                  bool is_gaia_picture) {
  icon_.reset(new gfx::Image(image));
  is_gaia_picture_ = is_gaia_picture;
  UpdateButtonIcon();
}

gboolean AvatarMenuButtonGtk::OnButtonPressed(GtkWidget* widget,
                                              GdkEventButton* event) {
  if (event->button != 1)
    return FALSE;

  ShowAvatarBubble();
  ProfileMetrics::LogProfileOpenMethod(ProfileMetrics::ICON_AVATAR_BUBBLE);
  return TRUE;
}

void AvatarMenuButtonGtk::OnSizeAllocate(GtkWidget* widget,
                                         GtkAllocation* allocation) {
  if (allocation->height != old_height_)
    UpdateButtonIcon();
}

void AvatarMenuButtonGtk::ShowAvatarBubble() {
  new AvatarMenuBubbleGtk(browser_, widget_.get(), arrow_location_, NULL);
}

void AvatarMenuButtonGtk::UpdateButtonIcon() {
  if (!icon_.get())
    return;

  old_height_ = widget()->allocation.height;
  gfx::Image icon = profiles::GetAvatarIconForTitleBar(*icon_, is_gaia_picture_,
      profiles::kAvatarIconWidth, old_height_);
  gtk_image_set_from_pixbuf(GTK_IMAGE(image_), icon.ToGdkPixbuf());
  gtk_misc_set_alignment(GTK_MISC(image_), 0.0, 1.0);
  gtk_widget_set_size_request(image_, -1, 0);
}
