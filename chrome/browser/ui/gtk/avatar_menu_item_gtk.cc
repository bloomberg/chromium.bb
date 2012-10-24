// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/avatar_menu_item_gtk.h"

#include <gdk/gdkkeysyms.h>

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_source.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/image/image.h"

namespace {

// A checkmark is drawn in the lower-right corner of the active avatar image.
// This value is the x offset, in pixels, from that position.
const int kCheckMarkXOffset = 2;

// The maximum width of a user name in pixels. Anything longer than this will be
// elided.
const int kUserNameMaxWidth = 200;

// The color of the item highlight when we're in chrome-theme mode.
const GdkColor kHighlightColor = GDK_COLOR_RGB(0xe3, 0xed, 0xf6);

// The color of the background when we're in chrome-theme mode.
const GdkColor kBackgroundColor = GDK_COLOR_RGB(0xff, 0xff, 0xff);

}  // namespace

AvatarMenuItemGtk::AvatarMenuItemGtk(Delegate* delegate,
                                     const AvatarMenuModel::Item& item,
                                     size_t item_index,
                                     GtkThemeService* theme_service)
    : delegate_(delegate),
      item_(item),
      item_index_(item_index),
      theme_service_(theme_service),
      status_label_(NULL),
      link_alignment_(NULL),
      edit_profile_link_(NULL) {
  Init(theme_service_);

  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(theme_service_));
  theme_service_->InitThemesFor(this);
}

AvatarMenuItemGtk::~AvatarMenuItemGtk() {
  widget_.Destroy();
}

gboolean AvatarMenuItemGtk::OnProfileClick(GtkWidget* widget,
                                           GdkEventButton* event) {
  delegate_->OpenProfile(item_index_);
  return FALSE;
}

gboolean AvatarMenuItemGtk::OnProfileKeyPress(GtkWidget* widget,
                                              GdkEventKey* event) {
  if (event->keyval == GDK_Return ||
      event->keyval == GDK_ISO_Enter ||
      event->keyval == GDK_KP_Enter) {
    if (item_.active)
      delegate_->EditProfile(item_index_);
    else
      delegate_->OpenProfile(item_index_);
  }

  return FALSE;
}

void AvatarMenuItemGtk::ShowStatusLabel() {
  gtk_widget_show(status_label_);
  gtk_widget_hide(link_alignment_);
}

void AvatarMenuItemGtk::ShowEditLink() {
  gtk_widget_hide(status_label_);
  gtk_widget_show(link_alignment_);
}

gboolean AvatarMenuItemGtk::OnProfileFocusIn(GtkWidget* widget,
                                             GdkEventFocus* event) {
  if (item_.active)
    ShowEditLink();

  return FALSE;
}

gboolean AvatarMenuItemGtk::OnProfileFocusOut(GtkWidget* widget,
                                              GdkEventFocus* event) {
  if (item_.active)
    ShowStatusLabel();

  return FALSE;
}

gboolean AvatarMenuItemGtk::OnProfileEnter(GtkWidget* widget,
                                           GdkEventCrossing* event) {
  if (event->detail == GDK_NOTIFY_INFERIOR)
    return FALSE;

  gtk_widget_modify_bg(widget, GTK_STATE_NORMAL, &highlighted_color_);
  if (item_.active)
    ShowEditLink();

  return FALSE;
}

gboolean AvatarMenuItemGtk::OnProfileLeave(GtkWidget* widget,
                                           GdkEventCrossing* event) {
  if (event->detail == GDK_NOTIFY_INFERIOR)
    return FALSE;

  gtk_widget_modify_bg(widget, GTK_STATE_NORMAL, unhighlighted_color_);
  if (item_.active)
    ShowStatusLabel();

  return FALSE;
}

void AvatarMenuItemGtk::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_BROWSER_THEME_CHANGED);
  bool using_native = theme_service_->UsingNativeTheme();

  if (using_native) {
    GtkStyle* style = gtk_rc_get_style(widget_.get());
    highlighted_color_ = style->bg[GTK_STATE_SELECTED];
    unhighlighted_color_ = NULL;
  } else {
    highlighted_color_ = kHighlightColor;
    unhighlighted_color_ = &kBackgroundColor;
  }

  // Assume that the widget isn't highlighted since theme changes will almost
  // never happen while we're up.
  gtk_widget_modify_bg(widget_.get(), GTK_STATE_NORMAL, unhighlighted_color_);
}

void AvatarMenuItemGtk::OnEditProfileLinkClicked(GtkWidget* link) {
  delegate_->EditProfile(item_index_);
}

gboolean AvatarMenuItemGtk::OnEventBoxExpose(GtkWidget* widget,
                                             GdkEventExpose* event) {
  // Draw the focus rectangle.
  if (gtk_widget_has_focus(widget)) {
    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    gtk_paint_focus(gtk_widget_get_style(widget),
                    gtk_widget_get_window(widget),
                    gtk_widget_get_state(widget),
                    &event->area, widget, NULL,
                    0, 0,
                    allocation.width, allocation.height);
  }

  return TRUE;
}

void AvatarMenuItemGtk::Init(GtkThemeService* theme_service) {
  widget_.Own(gtk_event_box_new());

  g_signal_connect(widget_.get(), "button-press-event",
      G_CALLBACK(OnProfileClickThunk), this);
  g_signal_connect(widget_.get(), "enter-notify-event",
      G_CALLBACK(OnProfileEnterThunk), this);
  g_signal_connect(widget_.get(), "leave-notify-event",
      G_CALLBACK(OnProfileLeaveThunk), this);
  g_signal_connect(widget_.get(), "focus-in-event",
      G_CALLBACK(OnProfileFocusInThunk), this);
  g_signal_connect(widget_.get(), "focus-out-event",
      G_CALLBACK(OnProfileFocusOutThunk), this);
  g_signal_connect(widget_.get(), "key-press-event",
      G_CALLBACK(OnProfileKeyPressThunk), this);
  g_signal_connect_after(widget_.get(), "expose-event",
      G_CALLBACK(OnEventBoxExposeThunk), this);

  GtkWidget* item_hbox = gtk_hbox_new(FALSE, ui::kControlSpacing);
  GdkPixbuf* avatar_pixbuf = NULL;

  // If this profile is active then draw a check mark on the bottom right
  // of the profile icon.
  if (item_.active) {
    const SkBitmap* avatar_image = item_.icon.ToSkBitmap();
    gfx::ImageSkiaRep avatar_image_rep =
        gfx::ImageSkiaRep(*avatar_image, ui::SCALE_FACTOR_100P);
    gfx::Canvas canvas(avatar_image_rep, /* is_opaque */ true);

    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    const gfx::ImageSkia* check_image = rb.GetImageNamed(
        IDR_PROFILE_SELECTED).ToImageSkia();
    gfx::Rect check_rect(0, 0, check_image->width(), check_image->height());
    int y = avatar_image->height() - check_image->height();
    int x = avatar_image->width() - check_image->width() + kCheckMarkXOffset;
    canvas.DrawImageInt(*check_image, x, y);

    SkBitmap final_image = canvas.ExtractImageRep().sk_bitmap();
    avatar_pixbuf = gfx::GdkPixbufFromSkBitmap(final_image);
  } else {
    avatar_pixbuf = gfx::GdkPixbufFromSkBitmap(*item_.icon.ToSkBitmap());
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
                                       ui::ELIDE_AT_END);

  name_label = theme_service->BuildLabel(UTF16ToUTF8(elided_name),
                                         ui::kGdkBlack);
  if (item_.active) {
    char* markup = g_markup_printf_escaped(
        "<span weight='bold'>%s</span>", UTF16ToUTF8(elided_name).c_str());
    gtk_label_set_markup(GTK_LABEL(name_label), markup);
  }

  gtk_misc_set_alignment(GTK_MISC(name_label), 0, 0);
  gtk_box_pack_start(GTK_BOX(item_vbox), name_label, TRUE, TRUE, 0);

  // The sync status label.
  status_label_ = theme_service_->BuildLabel(std::string(), ui::kGdkBlack);
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
    edit_profile_link_ = theme_service_->BuildChromeLinkButton(
        l10n_util::GetStringUTF8(IDS_PROFILES_EDIT_PROFILE_LINK));
    // Fix for bug#107348. edit link steals focus from menu item which
    // hides edit link button in focus-out-event handler,
    // so, it misses the click event.
    gtk_widget_set_can_focus(edit_profile_link_, FALSE);

    link_alignment_ = gtk_alignment_new(0, 0, 0, 0);
    gtk_container_add(GTK_CONTAINER(link_alignment_), edit_profile_link_);

    // The chrome link button contains a label that won't be shown if the button
    // is set to "no show all", so show all first.
    gtk_widget_show_all(link_alignment_);
    gtk_widget_set_no_show_all(link_alignment_, TRUE);
    gtk_widget_hide(link_alignment_);

    gtk_box_pack_start(GTK_BOX(item_vbox), link_alignment_, FALSE, FALSE, 0);

    g_signal_connect(edit_profile_link_, "clicked",
                     G_CALLBACK(OnEditProfileLinkClickedThunk), this);

    GtkSizeGroup* size_group = gtk_size_group_new(GTK_SIZE_GROUP_BOTH);
    gtk_size_group_add_widget(size_group, status_label_);
    gtk_size_group_add_widget(size_group, link_alignment_);
    g_object_unref(size_group);
  }

  gtk_box_pack_start(GTK_BOX(item_hbox), item_vbox, TRUE, TRUE, 0);
  gtk_container_add(GTK_CONTAINER(widget_.get()), item_hbox);
}
