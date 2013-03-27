// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/avatar_menu_bubble_gtk.h"

#include "base/i18n/rtl.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/avatar_menu_model.h"
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
#include "chrome/common/chrome_notification_types.h"
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
      theme_service_(GtkThemeService::GetFrom(browser->profile())),
      new_profile_link_(NULL),
      minimum_width_(kBubbleMinWidth) {
  avatar_menu_model_.reset(new AvatarMenuModel(
      &g_browser_process->profile_manager()->GetProfileInfoCache(),
      this, browser));

  InitContents();

  OnAvatarMenuModelChanged(avatar_menu_model_.get());

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
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void AvatarMenuBubbleGtk::BubbleClosing(BubbleGtk* bubble,
                                        bool closed_by_escape) {
  bubble_ = NULL;
}

void AvatarMenuBubbleGtk::OnAvatarMenuModelChanged(
    AvatarMenuModel* avatar_menu_model) {
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
  avatar_menu_model_->SwitchToProfile(profile_index,
      event_utils::DispositionFromGdkState(modifier_state_uint) == NEW_WINDOW);
  CloseBubble();
}

void AvatarMenuBubbleGtk::EditProfile(size_t profile_index) {
  if (!bubble_)
    return;
  avatar_menu_model_->EditProfile(profile_index);
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
  avatar_menu_model_->AddNewProfile(ProfileMetrics::ADD_NEW_USER_ICON);
  CloseBubble();
}

void AvatarMenuBubbleGtk::InitContents() {
  size_t profile_count = avatar_menu_model_->GetNumberOfItems();

  contents_ = gtk_vbox_new(FALSE, ui::kControlSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(contents_),
                                 ui::kContentAreaBorder);
  g_signal_connect(contents_, "size-request",
                   G_CALLBACK(OnSizeRequestThunk), this);

  GtkWidget* items_vbox = gtk_vbox_new(FALSE, ui::kContentAreaSpacing);

  for (size_t i = 0; i < profile_count; ++i) {
    AvatarMenuModel::Item menu_item = avatar_menu_model_->GetItemAt(i);
    AvatarMenuItemGtk* item = new AvatarMenuItemGtk(
        this, menu_item, i, theme_service_);
    items_.push_back(item);

    gtk_box_pack_start(GTK_BOX(items_vbox), item->widget(), TRUE, TRUE, 0);
    gtk_widget_set_can_focus(item->widget(), TRUE);
    if (menu_item.active)
      gtk_container_set_focus_child(GTK_CONTAINER(items_vbox), item->widget());
  }

  gtk_box_pack_start(GTK_BOX(contents_), items_vbox, TRUE, TRUE, 0);

  if (avatar_menu_model_->ShouldShowAddNewProfileLink()) {
    gtk_box_pack_start(GTK_BOX(contents_), gtk_hseparator_new(), TRUE, TRUE, 0);

    // The new profile link.
    new_profile_link_ = theme_service_->BuildChromeLinkButton(
        l10n_util::GetStringUTF8(IDS_PROFILES_CREATE_NEW_PROFILE_LINK));
    g_signal_connect(new_profile_link_, "clicked",
                     G_CALLBACK(OnNewProfileLinkClickedThunk), this);

    GtkWidget* link_align = gtk_alignment_new(0, 0, 0, 0);
    gtk_alignment_set_padding(GTK_ALIGNMENT(link_align),
                              0, 0, kNewProfileLinkLeftPadding, 0);
    gtk_container_add(GTK_CONTAINER(link_align), new_profile_link_);

    gtk_box_pack_start(GTK_BOX(contents_), link_align, FALSE, FALSE, 0);
  }
}

void AvatarMenuBubbleGtk::CloseBubble() {
  if (bubble_) {
    bubble_->Close();
    bubble_ = NULL;
  }
}
