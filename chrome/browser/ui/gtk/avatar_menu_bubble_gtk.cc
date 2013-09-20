// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/avatar_menu_bubble_gtk.h"

#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/gtk/avatar_menu_item_gtk.h"
#include "chrome/browser/ui/gtk/browser_toolbar_gtk.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/event_utils.h"
#include "chrome/browser/ui/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/location_bar_view_gtk.h"
#include "content/public/browser/notification_source.h"
#include "grit/generated_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// The minimum width in pixels of the bubble.
const int kBubbleMinWidth = 175;

// The number of pixels of padding on the left of the 'New Profile' link at the
// bottom of the bubble.
const int kNewProfileLinkLeftPadding = 40;

}  // namespace

AvatarMenuBubbleGtk::AvatarMenuBubbleGtk(Browser* browser,
                                         GtkWidget* anchor,
                                         BubbleGtk::FrameStyle arrow,
                                         const gfx::Rect* rect)
    : contents_(NULL),
      inner_contents_(NULL),
      theme_service_(GtkThemeService::GetFrom(browser->profile())),
      new_profile_link_(NULL),
      minimum_width_(kBubbleMinWidth),
      switching_(false) {
  avatar_menu_.reset(new AvatarMenu(
      &g_browser_process->profile_manager()->GetProfileInfoCache(),
      this, browser));
  avatar_menu_->RebuildMenu();

  OnAvatarMenuChanged(avatar_menu_.get());

  bubble_ = BubbleGtk::Show(anchor,
                            rect,
                            contents_,
                            arrow,
                            BubbleGtk::MATCH_SYSTEM_THEME |
                                BubbleGtk::POPUP_WINDOW |
                                BubbleGtk::GRAB_INPUT,
                            theme_service_,
                            this);  // |delegate|
  g_signal_connect(contents_, "destroy",
                   G_CALLBACK(&OnDestroyThunk), this);
}

AvatarMenuBubbleGtk::~AvatarMenuBubbleGtk() {}

void AvatarMenuBubbleGtk::OnDestroy(GtkWidget* widget) {
  // We are self deleting, we have a destroy signal setup to catch when we
  // destroyed (via the BubbleGtk being destroyed), and delete ourself.
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void AvatarMenuBubbleGtk::BubbleClosing(BubbleGtk* bubble,
                                        bool closed_by_escape) {
  bubble_ = NULL;
}

void AvatarMenuBubbleGtk::OnAvatarMenuChanged(
    AvatarMenu* avatar_menu) {
  items_.clear();
  minimum_width_ = kBubbleMinWidth;

  InitContents();
}

void AvatarMenuBubbleGtk::OpenProfile(size_t profile_index) {
  if (!bubble_)
    return;
  GdkModifierType modifier_state;
  gtk_get_current_event_state(&modifier_state);
  guint modifier_state_uint = modifier_state;
  avatar_menu_->SwitchToProfile(profile_index,
      event_utils::DispositionFromGdkState(modifier_state_uint) == NEW_WINDOW);
  CloseBubble();
}

void AvatarMenuBubbleGtk::EditProfile(size_t profile_index) {
  if (!bubble_)
    return;
  avatar_menu_->EditProfile(profile_index);
  CloseBubble();
}

void AvatarMenuBubbleGtk::OnSizeRequest(GtkWidget* widget,
                                        GtkRequisition* req) {
  // Always use the maximum width ever requested.
  if (req->width < minimum_width_)
    req->width = minimum_width_;
  else
    minimum_width_ = req->width;
}

void AvatarMenuBubbleGtk::OnNewProfileLinkClicked(GtkWidget* link) {
  if (!bubble_)
    return;
  avatar_menu_->AddNewProfile(ProfileMetrics::ADD_NEW_USER_ICON);
  CloseBubble();
}

void AvatarMenuBubbleGtk::OnSwitchProfileLinkClicked(GtkWidget* link) {
  switching_ = true;
  OnAvatarMenuChanged(avatar_menu_.get());
}

void AvatarMenuBubbleGtk::InitMenuContents() {
  size_t profile_count = avatar_menu_->GetNumberOfItems();
  GtkWidget* items_vbox = gtk_vbox_new(FALSE, ui::kContentAreaSpacing);
  for (size_t i = 0; i < profile_count; ++i) {
    AvatarMenu::Item menu_item = avatar_menu_->GetItemAt(i);
    AvatarMenuItemGtk* item = new AvatarMenuItemGtk(
        this, menu_item, i, theme_service_);
    items_.push_back(item);

    gtk_box_pack_start(GTK_BOX(items_vbox), item->widget(), TRUE, TRUE, 0);
    gtk_widget_set_can_focus(item->widget(), TRUE);
    if (menu_item.active)
      gtk_container_set_focus_child(GTK_CONTAINER(items_vbox), item->widget());
  }
  gtk_box_pack_start(GTK_BOX(inner_contents_), items_vbox, TRUE, TRUE, 0);

  if (avatar_menu_->ShouldShowAddNewProfileLink()) {
    gtk_box_pack_start(GTK_BOX(inner_contents_),
                       gtk_hseparator_new(), TRUE, TRUE, 0);

    // The new profile link.
    new_profile_link_ = theme_service_->BuildChromeLinkButton(
        l10n_util::GetStringUTF8(IDS_PROFILES_CREATE_NEW_PROFILE_LINK));
    g_signal_connect(new_profile_link_, "clicked",
                     G_CALLBACK(OnNewProfileLinkClickedThunk), this);

    GtkWidget* link_align = gtk_alignment_new(0, 0, 0, 0);
    gtk_alignment_set_padding(GTK_ALIGNMENT(link_align),
                              0, 0, kNewProfileLinkLeftPadding, 0);
    gtk_container_add(GTK_CONTAINER(link_align), new_profile_link_);

    gtk_box_pack_start(GTK_BOX(inner_contents_), link_align, FALSE, FALSE, 0);
  }
}

void AvatarMenuBubbleGtk::InitManagedUserContents() {
  int active_index = avatar_menu_->GetActiveProfileIndex();
  AvatarMenu::Item menu_item =
      avatar_menu_->GetItemAt(active_index);
  AvatarMenuItemGtk* item = new AvatarMenuItemGtk(
      this, menu_item, active_index, theme_service_);
  items_.push_back(item);

  gtk_box_pack_start(GTK_BOX(inner_contents_), item->widget(), TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(inner_contents_),
                     gtk_hseparator_new(), TRUE, TRUE, 0);

  // Add information about managed users within a hbox.
  GtkWidget* managed_user_info = gtk_hbox_new(FALSE, 5);
  GdkPixbuf* limited_user_pixbuf =
      avatar_menu_->GetManagedUserIcon().ToGdkPixbuf();
  GtkWidget* limited_user_img = gtk_image_new_from_pixbuf(limited_user_pixbuf);
  GtkWidget* icon_align = gtk_alignment_new(0, 0, 0, 0);
  gtk_container_add(GTK_CONTAINER(icon_align), limited_user_img);
  gtk_box_pack_start(GTK_BOX(managed_user_info), icon_align, FALSE, FALSE, 0);
  GtkWidget* status_label =
      theme_service_->BuildLabel(std::string(), ui::kGdkBlack);
  char* markup = g_markup_printf_escaped(
      "<span size='small'>%s</span>",
      UTF16ToUTF8(avatar_menu_->GetManagedUserInformation()).c_str());
  const int kLabelWidth = 150;
  gtk_widget_set_size_request(status_label, kLabelWidth, -1);
  gtk_label_set_markup(GTK_LABEL(status_label), markup);
  gtk_label_set_line_wrap(GTK_LABEL(status_label), TRUE);
  gtk_misc_set_alignment(GTK_MISC(status_label), 0, 0);
  g_free(markup);
  gtk_box_pack_start(GTK_BOX(managed_user_info), status_label, FALSE, FALSE, 0);
  gtk_box_pack_start(
      GTK_BOX(inner_contents_), managed_user_info, FALSE, FALSE, 0);

  gtk_box_pack_start(
      GTK_BOX(inner_contents_), gtk_hseparator_new(), TRUE, TRUE, 0);

  // The switch profile link.
  GtkWidget* switch_profile_link = theme_service_->BuildChromeLinkButton(
      l10n_util::GetStringUTF8(IDS_PROFILES_SWITCH_PROFILE_LINK));
  g_signal_connect(switch_profile_link, "clicked",
                   G_CALLBACK(OnSwitchProfileLinkClickedThunk), this);

  GtkWidget* link_align = gtk_alignment_new(0.5, 0, 0, 0);
  gtk_container_add(GTK_CONTAINER(link_align), switch_profile_link);

  gtk_box_pack_start(GTK_BOX(inner_contents_), link_align, FALSE, FALSE, 0);
}

void AvatarMenuBubbleGtk::InitContents() {
  // Destroy the old inner contents to allow replacing it.
  if (inner_contents_)
    gtk_widget_destroy(inner_contents_);
  inner_contents_ = gtk_vbox_new(FALSE, ui::kControlSpacing);
  if (!contents_)
    contents_ = gtk_vbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(inner_contents_),
                                 ui::kContentAreaBorder);
  g_signal_connect(inner_contents_, "size-request",
                   G_CALLBACK(OnSizeRequestThunk), this);

  if (avatar_menu_->GetManagedUserInformation().empty() || switching_)
    InitMenuContents();
  else
    InitManagedUserContents();
  gtk_box_pack_start(GTK_BOX(contents_), inner_contents_, TRUE, TRUE, 0);
  if (bubble_)
    gtk_widget_show_all(contents_);
}

void AvatarMenuBubbleGtk::CloseBubble() {
  if (bubble_) {
    bubble_->Close();
    bubble_ = NULL;
  }
}
