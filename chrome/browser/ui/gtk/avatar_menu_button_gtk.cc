// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/avatar_menu_button_gtk.h"

#include "base/i18n/rtl.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/gtk/avatar_menu_bubble_gtk.h"
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"
#include "ui/gfx/gtk_util.h"

AvatarMenuButtonGtk::AvatarMenuButtonGtk(Browser* browser)
    : image_(NULL),
      browser_(browser),
      frame_style_(BubbleGtk::ANCHOR_TOP_LEFT),
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
  DCHECK(chrome::IsCommandEnabled(browser_, IDC_SHOW_AVATAR_MENU));
  // Only show the avatar bubble if the avatar button is in the title bar.
  if (gtk_widget_get_parent_window(widget_.get()))
    new AvatarMenuBubbleGtk(browser_, widget_.get(), frame_style_, NULL);
}

void AvatarMenuButtonGtk::UpdateButtonIcon() {
  if (!icon_.get())
    return;

  GtkAllocation allocation;
  gtk_widget_get_allocation(widget(), &allocation);
  old_height_ = allocation.height;

  // GAIA images are square; use kWidth for both destination height and width
  // since old_height_ is often not usable (typically a value of 1 which, after
  // subtracting border, tries to resize the profile image to a size of -1).
  gfx::Image icon = profiles::GetAvatarIconForTitleBar(
      *icon_, is_gaia_picture_,
      profiles::kAvatarIconWidth, profiles::kAvatarIconWidth);
  gtk_image_set_from_pixbuf(GTK_IMAGE(image_), icon.ToGdkPixbuf());
  gtk_misc_set_alignment(GTK_MISC(image_), 0.0, 1.0);
  gtk_widget_set_size_request(image_, -1, 0);
}
