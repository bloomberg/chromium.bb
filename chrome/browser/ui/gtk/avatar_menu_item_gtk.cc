// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/avatar_menu_item_gtk.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/image/image.h"

namespace {

// A checkmark is drawn in the lower-right corner of the active avatar image.
// This value is the x offset, in pixels, from that position.
const int kCheckMarkXOffset = 2;

// The maximum width of a user name in pixels. Anything longer than this will be
// elided.
const int kUserNameMaxWidth = 200;

}  // namespace

AvatarMenuItemGtk::AvatarMenuItemGtk(Delegate* delegate,
                                     const AvatarMenuModel::Item& item,
                                     size_t item_index,
                                     GtkThemeService* theme_service)
    : delegate_(delegate),
      item_(item),
      item_index_(item_index),
      status_label_(NULL),
      link_alignment_(NULL) {
  Init(theme_service);
}

AvatarMenuItemGtk::~AvatarMenuItemGtk() {
}

gboolean AvatarMenuItemGtk::OnProfileClick(GtkWidget* widget,
                                           GdkEventButton* event) {
  delegate_->OpenProfile(item_index_);
  return FALSE;
}

gboolean AvatarMenuItemGtk::OnProfileEnter(GtkWidget* widget,
                                           GdkEventCrossing* event) {
  if (event->detail == GDK_NOTIFY_INFERIOR)
    return FALSE;

  GtkStyle* style = gtk_rc_get_style(widget);
  GdkColor highlight_color = style->bg[GTK_STATE_SELECTED];
  gtk_widget_modify_bg(widget, GTK_STATE_NORMAL, &highlight_color);
  if (item_.active) {
    gtk_widget_hide(status_label_);
    gtk_widget_show(link_alignment_);
  }

  return FALSE;
}

gboolean AvatarMenuItemGtk::OnProfileLeave(GtkWidget* widget,
                                           GdkEventCrossing* event) {
  if (event->detail == GDK_NOTIFY_INFERIOR)
    return FALSE;

  gtk_widget_modify_bg(widget, GTK_STATE_NORMAL, NULL);
  if (item_.active) {
    gtk_widget_show(status_label_);
    gtk_widget_hide(link_alignment_);
  }

  return FALSE;
}

void AvatarMenuItemGtk::OnEditProfileLinkClicked(GtkWidget* link) {
  delegate_->EditProfile(item_index_);
}

void AvatarMenuItemGtk::Init(GtkThemeService* theme_service) {
  widget_.Own(gtk_event_box_new());

  g_signal_connect(widget_.get(), "button-press-event",
      G_CALLBACK(OnProfileClickThunk), this);
  g_signal_connect(widget_.get(), "enter-notify-event",
      G_CALLBACK(OnProfileEnterThunk), this);
  g_signal_connect(widget_.get(), "leave-notify-event",
      G_CALLBACK(OnProfileLeaveThunk), this);

  GtkWidget* item_hbox = gtk_hbox_new(FALSE, ui::kControlSpacing);
  GdkPixbuf* avatar_pixbuf = NULL;

  // If this profile is active then draw a check mark on the bottom right
  // of the profile icon.
  if (item_.active) {
    const SkBitmap* avatar_image = item_.icon.ToSkBitmap();
    gfx::CanvasSkia canvas(avatar_image->width(),
                           avatar_image->height(),
                           /* is_opaque */ true);
    canvas.DrawBitmapInt(*avatar_image, 0, 0);

    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    SkBitmap check_image = rb.GetImageNamed(IDR_PROFILE_SELECTED);
    gfx::Rect check_rect(0, 0, check_image.width(), check_image.height());
    int y = avatar_image->height() - check_image.height();
    int x = avatar_image->width() - check_image.width() + kCheckMarkXOffset;
    canvas.DrawBitmapInt(check_image, x, y);

    SkBitmap final_image = canvas.ExtractBitmap();
    avatar_pixbuf = gfx::GdkPixbufFromSkBitmap(&final_image);
  } else {
    avatar_pixbuf = gfx::GdkPixbufFromSkBitmap(item_.icon.ToSkBitmap());
  }

  GtkWidget* avatar_image = gtk_image_new_from_pixbuf(avatar_pixbuf);
  g_object_unref(avatar_pixbuf);
  gtk_misc_set_alignment(GTK_MISC(avatar_image), 0, 0);
  gtk_box_pack_start(GTK_BOX(item_hbox), avatar_image, FALSE, FALSE, 0);

  // The user name label.
  GtkWidget* item_vbox = gtk_vbox_new(FALSE, 0);
  GtkWidget* name_label = NULL;
  string16 elided_name = ui::ElideText(item_.name,
                                       gfx::Font(),
                                       kUserNameMaxWidth,
                                       /* elide_in_middle */ false);
  if (item_.active) {
    name_label = gtk_util::CreateBoldLabel(UTF16ToUTF8(elided_name));
  } else {
    name_label = theme_service->BuildLabel(UTF16ToUTF8(elided_name),
                                           ui::kGdkBlack);
  }
  gtk_misc_set_alignment(GTK_MISC(name_label), 0, 0);
  gtk_box_pack_start(GTK_BOX(item_vbox), name_label, TRUE, TRUE, 0);

  // The sync status label.
  status_label_ = gtk_label_new("");
  char* markup = g_markup_printf_escaped(
      "<span size='small'>%s</span>", UTF16ToUTF8(item_.sync_state).c_str());
  gtk_label_set_markup(GTK_LABEL(status_label_), markup);
  g_free(markup);
  gtk_misc_set_alignment(GTK_MISC(status_label_), 0, 0);
  gtk_widget_set_no_show_all(status_label_, TRUE);
  gtk_box_pack_start(GTK_BOX(item_vbox), status_label_, FALSE, FALSE, 0);
  gtk_widget_show(status_label_);

  if (item_.active) {
    // The "edit your profile" link.
    GtkWidget* edit_profile_link = gtk_chrome_link_button_new(
        l10n_util::GetStringUTF8(IDS_PROFILES_EDIT_PROFILE_LINK).c_str());

    link_alignment_ = gtk_alignment_new(0, 0, 0, 0);
    gtk_container_add(GTK_CONTAINER(link_alignment_), edit_profile_link);

    // The chrome link button contains a label that won't be shown if the button
    // is set to "no show all", so show all first.
    gtk_widget_show_all(link_alignment_);
    gtk_widget_set_no_show_all(link_alignment_, TRUE);
    gtk_widget_hide(link_alignment_);

    gtk_box_pack_start(GTK_BOX(item_vbox), link_alignment_, FALSE, FALSE, 0);

    g_signal_connect(edit_profile_link, "clicked",
                     G_CALLBACK(OnEditProfileLinkClickedThunk), this);

    GtkSizeGroup* size_group = gtk_size_group_new(GTK_SIZE_GROUP_BOTH);
    gtk_size_group_add_widget(size_group, status_label_);
    gtk_size_group_add_widget(size_group, link_alignment_);
    g_object_unref(size_group);
  }

  gtk_box_pack_start(GTK_BOX(item_hbox), item_vbox, TRUE, TRUE, 0);
  gtk_container_add(GTK_CONTAINER(widget_.get()), item_hbox);
}
