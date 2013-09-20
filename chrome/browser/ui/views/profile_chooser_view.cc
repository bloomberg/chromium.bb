// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profile_chooser_view.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_info_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/views/user_manager_view.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

// Helpers --------------------------------------------------------------------

namespace {

const int kLargeImageSide = 64;
const int kSmallImageSide = 32;
const int kMinMenuWidth = 250;

// Current profile avatar image.
views::View* CreateProfileImageView(const gfx::Image& icon) {
  views::ImageView* view = new views::ImageView();

  gfx::Image image = profiles::GetSizedAvatarIconWithBorder(
      icon, true,
      kLargeImageSide + profiles::kAvatarIconBorder,
      kLargeImageSide + profiles::kAvatarIconBorder);
  view->SetImage(image.ToImageSkia());

  return view;
}

// Creates a GridLayout with a single column. This ensures that all the child
// views added get auto-expanded to fill the full width of the bubble.
views::GridLayout* CreateSingleColumnLayout(views::View* view) {
  views::GridLayout* layout = new views::GridLayout(view);
  view->SetLayoutManager(layout);

  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                     views::GridLayout::USE_PREF, 0, 0);
  return layout;
}

// Creates a GridLayout with two columns.
views::GridLayout* CreateDoubleColumnLayout(views::View* view) {
  views::GridLayout* layout = new views::GridLayout(view);
  view->SetLayoutManager(layout);

  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 0,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(0, views::kUnrelatedControlHorizontalSpacing);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::TRAILING, 1,
                     views::GridLayout::USE_PREF, 0, 0);
  return layout;
}

views::Link* CreateLink(const string16 link_text,
                        views::LinkListener* listener) {
  views::Link* link_button = new views::Link(link_text);
  link_button->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  link_button->SetUnderline(false);
  link_button->set_listener(listener);
  return link_button;
}

}  // namespace


// ProfileChooserView ---------------------------------------------------------

// static
ProfileChooserView* ProfileChooserView::profile_bubble_ = NULL;
bool ProfileChooserView::close_on_deactivate_ = true;

// static
void ProfileChooserView::ShowBubble(
    views::View* anchor_view,
    views::BubbleBorder::Arrow arrow,
    views::BubbleBorder::BubbleAlignment border_alignment,
    const gfx::Rect& anchor_rect,
    Browser* browser) {
  if (IsShowing())
    // TODO(bcwhite): handle case where we should show on different window
    return;

  profile_bubble_ = new ProfileChooserView(
      anchor_view, arrow, anchor_rect, browser);
  views::BubbleDelegateView::CreateBubble(profile_bubble_);
  profile_bubble_->set_close_on_deactivate(close_on_deactivate_);
  profile_bubble_->SetAlignment(border_alignment);
  profile_bubble_->GetWidget()->Show();
}

// static
bool ProfileChooserView::IsShowing() {
  return profile_bubble_ != NULL;
}

// static
void ProfileChooserView::Hide() {
  if (IsShowing())
    profile_bubble_->GetWidget()->Close();
}

ProfileChooserView::ProfileChooserView(views::View* anchor_view,
                                       views::BubbleBorder::Arrow arrow,
                                       const gfx::Rect& anchor_rect,
                                       Browser* browser)
    : BubbleDelegateView(anchor_view, arrow),
      browser_(browser) {
  // Reset the default margins inherited from the BubbleDelegateView.
  set_margins(gfx::Insets());

  ResetLinksAndButtons();

  avatar_menu_.reset(new AvatarMenu(
      &g_browser_process->profile_manager()->GetProfileInfoCache(),
      this,
      browser_));
  avatar_menu_->RebuildMenu();
}

ProfileChooserView::~ProfileChooserView() {
}

void ProfileChooserView::ResetLinksAndButtons() {
  manage_accounts_link_ = NULL;
  signout_current_profile_link_ = NULL;
  signin_current_profile_link_ = NULL;
  change_photo_link_ = NULL;
  guest_button_ = NULL;
  end_guest_button_ = NULL;
  users_button_ = NULL;
  open_other_profile_indexes_map_.clear();
}

void ProfileChooserView::Init() {
  ShowView(PROFILE_CHOOSER_VIEW, avatar_menu_.get());
}

void ProfileChooserView::OnAvatarMenuChanged(
    AvatarMenu* avatar_menu) {
  // Refresh the view with the new menu. We can't just update the local copy
  // as this may have been triggered by a sign out action, in which case
  // the view is being destroyed.
  ShowView(PROFILE_CHOOSER_VIEW, avatar_menu);
}

void ProfileChooserView::ShowView(BubbleViewMode view_to_display,
                                  AvatarMenu* avatar_menu) {
  // The account management view should only be displayed if the active profile
  // is signed in.
  if (view_to_display == ACCOUNT_MANAGEMENT_VIEW) {
    const AvatarMenu::Item& active_item = avatar_menu->GetItemAt(
        avatar_menu->GetActiveProfileIndex());
    DCHECK(active_item.signed_in);
  }

  ResetLinksAndButtons();
  RemoveAllChildViews(true);

  views::GridLayout* layout = CreateSingleColumnLayout(this);
  layout->set_minimum_size(gfx::Size(kMinMenuWidth, 0));

  // Separate items into active and alternatives.
  Indexes other_profiles;
  bool is_guest_view = true;
  views::View* current_profile_view = NULL;
  views::View* current_profile_accounts = NULL;
  for (size_t i = 0; i < avatar_menu->GetNumberOfItems(); ++i) {
    const AvatarMenu::Item& item = avatar_menu->GetItemAt(i);
    if (item.active) {
      if (view_to_display == PROFILE_CHOOSER_VIEW) {
        current_profile_view = CreateCurrentProfileView(item, false);
      } else {
        current_profile_view = CreateCurrentProfileEditableView(item);
        current_profile_accounts = CreateCurrentProfileAccountsView(item);
      }
      is_guest_view = false;
    } else {
      other_profiles.push_back(i);
    }
  }

  if (!current_profile_view)  // Guest windows don't have an active profile.
    current_profile_view = CreateGuestProfileView();

  layout->StartRow(1, 0);
  layout->AddView(current_profile_view);

  if (view_to_display == PROFILE_CHOOSER_VIEW) {
    layout->StartRow(1, 0);
    layout->AddView(CreateOtherProfilesView(other_profiles));
  } else {
    DCHECK(current_profile_accounts);
    layout->StartRow(0, 0);
    layout->AddView(new views::Separator(views::Separator::HORIZONTAL));
    layout->StartRow(1, 0);
    layout->AddView(current_profile_accounts);
  }

  layout->StartRow(0, 0);
  layout->AddView(new views::Separator(views::Separator::HORIZONTAL));

  // Action buttons.
  views::View* option_buttons_view = CreateOptionsView(is_guest_view);
  layout->StartRow(0, 0);
  layout->AddView(option_buttons_view);

  Layout();
}

void ProfileChooserView::WindowClosing() {
  DCHECK_EQ(profile_bubble_, this);
  profile_bubble_ = NULL;
}

void ProfileChooserView::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  // Disable button after clicking so that it doesn't get clicked twice and
  // start a second action... which can crash Chrome.  But don't disable if it
  // has no parent (like in tests) because that will also crash.
  if (sender->parent())
    sender->SetEnabled(false);

  if (sender == guest_button_) {
    avatar_menu_->SwitchToGuestProfileWindow(browser_);
  } else if (sender == end_guest_button_) {
    profiles::CloseGuestProfileWindows();
  } else if (sender == users_button_) {
    UserManagerView::Show(browser_);
  } else {
    // One of the "other profiles" buttons was pressed.
    ButtonIndexes::const_iterator match =
        open_other_profile_indexes_map_.find(sender);
    DCHECK(match != open_other_profile_indexes_map_.end());
    avatar_menu_->SwitchToProfile(
        match->second,
        ui::DispositionFromEventFlags(event.flags()) == NEW_WINDOW);
  }
}

void ProfileChooserView::LinkClicked(views::Link* sender, int event_flags) {
  if (sender == manage_accounts_link_) {
    // ShowView() will DCHECK if this view is displayed for non signed-in users.
    ShowView(ACCOUNT_MANAGEMENT_VIEW, avatar_menu_.get());
    SizeToContents();   // The account list changes the height of the bubble.
  } else if (sender == signout_current_profile_link_) {
    avatar_menu_->BeginSignOut();
  } else if (sender == signin_current_profile_link_) {
    GURL page = signin::GetPromoURL(signin::SOURCE_MENU, false);
    chrome::ShowSingletonTab(browser_, page);
  } else {
    DCHECK(sender == change_photo_link_);
    avatar_menu_->EditProfile(
        avatar_menu_->GetActiveProfileIndex());
  }
}

views::View* ProfileChooserView::CreateCurrentProfileView(
    const AvatarMenu::Item& avatar_item,
    bool is_guest) {
  views::View* view = new views::View();
  views::GridLayout* layout = CreateDoubleColumnLayout(view);
  layout->SetInsets(views::kButtonHEdgeMarginNew,
                    views::kButtonVEdgeMarginNew,
                    views::kButtonHEdgeMarginNew,
                    views::kButtonVEdgeMarginNew);

  views::View* photo_image = CreateProfileImageView(avatar_item.icon);
  view->SetBoundsRect(photo_image->bounds());

  views::Label* name_label =
      new views::Label(avatar_item.name,
                       ui::ResourceBundle::GetSharedInstance().GetFont(
                           ui::ResourceBundle::MediumFont));
  name_label->SetElideBehavior(views::Label::ELIDE_AT_END);
  name_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  layout->StartRow(1, 0);
  layout->AddView(photo_image, 1, 3);
  layout->AddView(name_label);

  if (is_guest) {
    layout->StartRow(1, 0);
    layout->SkipColumns(1);
    layout->StartRow(1, 0);
    layout->SkipColumns(1);
  } else if (avatar_item.signed_in) {
    manage_accounts_link_ = CreateLink(
        l10n_util::GetStringUTF16(IDS_PROFILES_PROFILE_MANAGE_ACCOUNTS_BUTTON),
        this);
    signout_current_profile_link_ = CreateLink(
        l10n_util::GetStringUTF16(IDS_PROFILES_PROFILE_SIGNOUT_BUTTON), this);
    layout->StartRow(1, 0);
    layout->SkipColumns(1);
    layout->AddView(manage_accounts_link_);
    layout->StartRow(1, 0);
    layout->SkipColumns(1);
    layout->AddView(signout_current_profile_link_);
  } else {
    signin_current_profile_link_ = CreateLink(
        l10n_util::GetStringFUTF16(
            IDS_SYNC_START_SYNC_BUTTON_LABEL,
            l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME)),
        this);
    layout->StartRow(1, 0);
    layout->SkipColumns(1);
    layout->AddView(signin_current_profile_link_);
    layout->StartRow(1, 0);
    layout->SkipColumns(1);
  }

  return view;
}

views::View* ProfileChooserView::CreateCurrentProfileEditableView(
    const AvatarMenu::Item& avatar_item) {
  DCHECK(avatar_item.signed_in);
  views::View* view = new views::View();
  views::GridLayout* layout = CreateDoubleColumnLayout(view);
  layout->SetInsets(views::kButtonHEdgeMarginNew,
                    views::kButtonVEdgeMarginNew,
                    views::kButtonHEdgeMarginNew,
                    views::kButtonVEdgeMarginNew);

  views::View* photo_image = CreateProfileImageView(avatar_item.icon);
  view->SetBoundsRect(photo_image->bounds());

  // TODO(noms): The name should actually be a textbox and not a label when
  // we have the functionality to save changes.
  views::Label* name_label =
      new views::Label(avatar_item.name,
                       ui::ResourceBundle::GetSharedInstance().GetFont(
                           ui::ResourceBundle::MediumFont));
  name_label->SetElideBehavior(views::Label::ELIDE_AT_END);
  name_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  change_photo_link_ = CreateLink(
      l10n_util::GetStringUTF16(IDS_PROFILES_PROFILE_CHANGE_PHOTO_BUTTON),
      this);

  layout->StartRow(1, 0);
  layout->AddView(photo_image, 1, 3);
  layout->AddView(name_label);

  layout->StartRow(1, 0);
  layout->SkipColumns(1);
  layout->AddView(change_photo_link_);

  layout->StartRow(1, 0);
  layout->SkipColumns(1);
  return view;
}

views::View* ProfileChooserView::CreateGuestProfileView() {
  gfx::Image guest_icon =
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(IDR_GUEST_ICON);
  AvatarMenu::Item guest_avatar_item(0, 0, guest_icon);
  guest_avatar_item.active = true;
  guest_avatar_item.name = l10n_util::GetStringUTF16(
      IDS_PROFILES_GUEST_PROFILE_NAME);
  guest_avatar_item.signed_in = false;

  return CreateCurrentProfileView(guest_avatar_item, true);
}

views::View* ProfileChooserView::CreateOtherProfilesView(
    const Indexes& avatars_to_show) {
  views::View* view = new views::View();
  views::GridLayout* layout = CreateSingleColumnLayout(view);
  layout->SetInsets(0 - views::kRelatedControlHorizontalSpacing,
                    views::kButtonVEdgeMarginNew,
                    views::kButtonHEdgeMarginNew,
                    views::kButtonVEdgeMarginNew);

  for (Indexes::const_iterator iter = avatars_to_show.begin();
       iter != avatars_to_show.end();
       ++iter) {
    const size_t index = *iter;
    const AvatarMenu::Item& item = avatar_menu_->GetItemAt(index);

    gfx::Image image = profiles::GetSizedAvatarIconWithBorder(
        item.icon, true,
        kSmallImageSide + profiles::kAvatarIconBorder,
        kSmallImageSide + profiles::kAvatarIconBorder);

    views::TextButton* button = new views::TextButton(this, item.name);
    open_other_profile_indexes_map_[button] = index;
    button->SetIcon(*image.ToImageSkia());
    button->SetFont(ui::ResourceBundle::GetSharedInstance().GetFont(
        ui::ResourceBundle::BaseFont));
    button->set_border(NULL);

    layout->AddPaddingRow(0, views::kRelatedControlHorizontalSpacing);
    layout->StartRow(1, 0);
    layout->AddView(button);
  }

  return view;
}

views::View* ProfileChooserView::CreateOptionsView(bool is_guest_view) {
  views::View* view = new views::View();
  views::GridLayout* layout = CreateSingleColumnLayout(view);
  layout->SetInsets(views::kRelatedControlHorizontalSpacing,
                    views::kButtonVEdgeMarginNew,
                    views::kButtonHEdgeMarginNew,
                    views::kButtonVEdgeMarginNew);

  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();

  layout->StartRow(1, 0);
  if (is_guest_view) {
    end_guest_button_ = new views::TextButton(this,
        l10n_util::GetStringUTF16(IDS_PROFILES_PROFILE_EXIT_GUEST_BUTTON));
    end_guest_button_->SetIcon(
        *rb->GetImageSkiaNamed(IDR_ICON_GUEST_WHITE));
    end_guest_button_->set_border(NULL);
    layout->AddView(end_guest_button_);
  } else {
    guest_button_ = new views::TextButton(this,
        l10n_util::GetStringUTF16(IDS_PROFILES_PROFILE_GUEST_BUTTON));
    guest_button_->SetIcon(*rb->GetImageSkiaNamed(IDR_ICON_GUEST_WHITE));
    guest_button_->set_border(NULL);
    layout->AddView(guest_button_);
  }

  users_button_ = new views::TextButton(this,
      l10n_util::GetStringUTF16(IDS_PROFILES_PROFILE_USERS_BUTTON));
  users_button_->SetIcon(*rb->GetImageSkiaNamed(IDR_ICON_ADD_USER_WHITE));
  users_button_->set_border(NULL);

  layout->AddPaddingRow(0, views::kRelatedControlHorizontalSpacing);
  layout->StartRow(1, 0);
  layout->AddView(users_button_);

  return view;
}

views::View* ProfileChooserView::CreateCurrentProfileAccountsView(
    const AvatarMenu::Item& avatar_item) {
  DCHECK(avatar_item.signed_in);
  views::View* view = new views::View();
  views::GridLayout* layout = CreateSingleColumnLayout(view);
  layout->SetInsets(views::kButtonHEdgeMarginNew,
                    views::kButtonVEdgeMarginNew,
                    views::kButtonHEdgeMarginNew,
                    views::kButtonVEdgeMarginNew);

  views::Label* email_label = new views::Label(avatar_item.sync_state);
  email_label->SetElideBehavior(views::Label::ELIDE_AS_EMAIL);
  email_label->SetFont(ui::ResourceBundle::GetSharedInstance().GetFont(
      ui::ResourceBundle::SmallFont));
  email_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  layout->StartRow(1, 0);
  layout->AddView(email_label);
  layout->AddPaddingRow(0, views::kUnrelatedControlHorizontalSpacing);

  views::BlueButton* add_account_button = new views::BlueButton(
      NULL,
      l10n_util::GetStringFUTF16(IDS_PROFILES_PROFILE_ADD_ACCOUNT_BUTTON,
                                 avatar_item.name));
  layout->StartRow(1, 0);
  layout->AddView(add_account_button);
  return view;
}
