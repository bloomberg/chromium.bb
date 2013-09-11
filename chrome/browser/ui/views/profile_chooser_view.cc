// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profile_chooser_view.h"

#include <vector>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/avatar_menu_model.h"
#include "chrome/browser/profiles/profile_info_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/user_manager_view.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"


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

ProfileChooserView::ProfileChooserView(
    views::View* anchor_view,
    views::BubbleBorder::Arrow arrow,
    const gfx::Rect& anchor_rect,
    Browser* browser)
    : BubbleDelegateView(anchor_view, arrow),
      browser_(browser),
      current_profile_view_(NULL),
      guest_button_view_(NULL),
      users_button_view_(NULL),
      other_profiles_view_(NULL),
      signout_current_profile_view_(NULL) {
  avatar_menu_model_.reset(new AvatarMenuModel(
      &g_browser_process->profile_manager()->GetProfileInfoCache(),
      this, browser_));
}

ProfileChooserView::~ProfileChooserView() {
}

void ProfileChooserView::Init() {
  // Build the menu for the first time.
  OnAvatarMenuModelChanged(avatar_menu_model_.get());
}

void ProfileChooserView::WindowClosing() {
  DCHECK_EQ(profile_bubble_, this);
  profile_bubble_ = NULL;
}

bool ProfileChooserView::OnMousePressed(const ui::MouseEvent& event) {
  return true;  // Won't get "released" event if we don't return "true" here.
}

void ProfileChooserView::OnMouseReleased(const ui::MouseEvent& event) {
  views::View* sender = GetEventHandlerForPoint(event.location());
  ViewIndexes::const_iterator match;

  match = open_other_profile_indexes_map_.find(sender);
  if (match != open_other_profile_indexes_map_.end()) {
    avatar_menu_model_->SwitchToProfile(
        match->second,
        ui::DispositionFromEventFlags(event.flags()) == NEW_WINDOW);
  }
}

void ProfileChooserView::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  // Disable button after clicking so that it doesn't get clicked twice and
  // start a second action... which can crash Chrome.  But don't disable if it
  // has no parent (like in tests) because that will also crash.
  if (sender->parent())
    sender->SetEnabled(false);

  if (sender == guest_button_view_) {
    avatar_menu_model_->SwitchToGuestProfileWindow(browser_);
  } else if (sender == users_button_view_) {
    UserManagerView::Show(browser_);
  } else {
    DCHECK_EQ(sender, signout_current_profile_view_);
    avatar_menu_model_->BeginSignOut();
  }
}

void ProfileChooserView::OnAvatarMenuModelChanged(
    AvatarMenuModel* avatar_menu_model) {
  // Unset all our child view references and call RemoveAllChildViews() which
  // will actually delete them.
  current_profile_view_ = NULL;
  guest_button_view_ = NULL;
  users_button_view_ = NULL;
  open_other_profile_indexes_map_.clear();
  other_profiles_view_ = NULL;
  signout_current_profile_view_ = NULL;
  RemoveAllChildViews(true);

  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 0,
                     views::GridLayout::USE_PREF, 0, 0);

  // Seprate items into active and alternatives.
  Indexes other_profiles;
  for (size_t i = 0; i < avatar_menu_model->GetNumberOfItems(); ++i) {
    const AvatarMenuModel::Item& item = avatar_menu_model->GetItemAt(i);
    if (item.active) {
      DCHECK(!current_profile_view_);
      current_profile_view_ = CreateCurrentProfileView(i);
    } else {
      other_profiles.push_back(i);
    }
  }
  if (!current_profile_view_)  // Guest windows don't have an entry.
    current_profile_view_ = CreateGuestProfileView();

  layout->StartRow(1, 0);
  layout->AddView(current_profile_view_);
  layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);

  layout->StartRow(0, 0);
  layout->AddView(new views::Separator(views::Separator::HORIZONTAL));

  other_profiles_view_ = CreateOtherProfilesView(other_profiles);
  layout->StartRow(1, 0);
  layout->AddView(other_profiles_view_);

  layout->StartRow(0, 0);
  layout->AddView(new views::Separator(views::Separator::HORIZONTAL));

  views::View* option_buttons_view = CreateOptionsView();
  option_buttons_view->SetSize(current_profile_view_->GetPreferredSize());
  layout->StartRow(0, 0);
  layout->AddView(option_buttons_view);

  layout->StartRow(0, 0);
  layout->AddView(new views::Separator(views::Separator::HORIZONTAL));

  // If the bubble has already been shown then resize and reposition the bubble.
  Layout();
}

views::View* ProfileChooserView::CreateProfileImageView(const gfx::Image& icon,
                                                        int side) {
  views::ImageView* view = new views::ImageView();

  gfx::Image image = profiles::GetSizedAvatarIconWithBorder(
      icon, true,
      side + profiles::kAvatarIconBorder,
      side + profiles::kAvatarIconBorder);
  // TODO(bcwhite): image alterations
  view->SetImage(image.ToImageSkia());

  return view;
}

views::View* ProfileChooserView::CreateProfileCardView(
    const AvatarMenuModel::Item& avatar_item) {
  views::View* view = new views::View();

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  const int kLargeImageSide = 64;
  views::View* photo_image =
      CreateProfileImageView(avatar_item.icon, kLargeImageSide);
  view->SetBoundsRect(photo_image->bounds());

  views::Label* name_label =
      new views::Label(avatar_item.name,
                       rb.GetFont(ui::ResourceBundle::MediumFont));
  name_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  views::Label* email_label = new views::Label(avatar_item.sync_state);
  if (avatar_item.signed_in)
    email_label->SetElideBehavior(views::Label::ELIDE_AS_EMAIL);
  email_label->SetFont(rb.GetFont(ui::ResourceBundle::SmallFont));
  email_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  views::GridLayout* layout = new views::GridLayout(view);
  view->SetLayoutManager(layout);

  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 0,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(0, views::kUnrelatedControlHorizontalSpacing);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::TRAILING, 1,
                     views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(1, 0);
  layout->AddView(photo_image, 1, 3);
  layout->AddView(name_label);
  layout->StartRow(1, 0);
  layout->SkipColumns(1);
  layout->AddView(email_label, 1, 1,
                  views::GridLayout::LEADING, views::GridLayout::LEADING);
  layout->StartRow(1, 0);
  layout->SkipColumns(1);

  return view;
}

views::View* ProfileChooserView::CreateCurrentProfileView(
    size_t avatar_to_show) {
  views::View* view = new views::View();

  views::View* card_view = CreateProfileCardView(
      avatar_menu_model_->GetItemAt(avatar_to_show));

  views::LabelButton* signout_button = new views::LabelButton(
      this,
      l10n_util::GetStringUTF16(IDS_PROFILES_PROFILE_SIGNOUT_BUTTON));
  signout_button->SetStyle(views::Button::STYLE_BUTTON);
  DCHECK(!signout_current_profile_view_);
  signout_current_profile_view_ = signout_button;

  views::GridLayout* layout = new views::GridLayout(view);
  view->SetLayoutManager(layout);

  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 0,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(0, 30);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 1,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(0, 10);

  layout->StartRow(1, 0);
  layout->AddView(card_view, 1, 1);
  layout->AddView(signout_button);

  return view;
}

views::View* ProfileChooserView::CreateGuestProfileView() {
  views::View* view = new views::View();

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  gfx::Image guest_icon = rb.GetImageNamed(IDR_GUEST_ICON);
  AvatarMenuModel::Item guest_avatar_item(0, guest_icon);
  guest_avatar_item.active = true;
  guest_avatar_item.name = l10n_util::GetStringUTF16(
      IDS_PROFILES_PROFILE_GUEST_BUTTON);
  guest_avatar_item.signed_in = false;

  views::View* card_view = CreateProfileCardView(guest_avatar_item);
  views::LabelButton* signout_button = new views::LabelButton(
      this,
      l10n_util::GetStringUTF16(IDS_PROFILES_PROFILE_SIGNOUT_BUTTON));
  signout_button->SetStyle(views::Button::STYLE_BUTTON);
  DCHECK(!signout_current_profile_view_);
  signout_current_profile_view_ = signout_button;

  views::GridLayout* layout = new views::GridLayout(view);
  view->SetLayoutManager(layout);

  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 0,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(0, 30);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 1,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(0, 10);

  layout->StartRow(1, 0);
  layout->AddView(card_view, 1, 1);
  layout->AddView(signout_button);

  return view;
}

views::View* ProfileChooserView::CreateOtherProfilesView(
    const Indexes& avatars_to_show) {
  views::View* view = new views::View();

  views::BoxLayout* layout = new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0,
      views::kRelatedControlSmallVerticalSpacing,
      views::kRelatedButtonHSpacing);
  view->SetLayoutManager(layout);

  const int kSmallImageSide = 32;
  for (Indexes::const_iterator iter = avatars_to_show.begin();
       iter != avatars_to_show.end();
       ++iter) {
    const size_t index = *iter;
    views::View* image = CreateProfileImageView(
        avatar_menu_model_->GetItemAt(index).icon, kSmallImageSide);
    open_other_profile_indexes_map_[image] = index;
    view->AddChildView(image);
  }

  return view;
}

views::View* ProfileChooserView::CreateOptionsView() {
  users_button_view_ = new views::LabelButton(
      this,
      l10n_util::GetStringUTF16(IDS_PROFILES_PROFILE_USERS_BUTTON));
  users_button_view_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  users_button_view_->set_tag(IDS_PROFILES_PROFILE_USERS_BUTTON);

 guest_button_view_ = new views::LabelButton(
      this,
      l10n_util::GetStringUTF16(IDS_PROFILES_PROFILE_GUEST_BUTTON));
  guest_button_view_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  guest_button_view_->set_tag(IDS_PROFILES_PROFILE_GUEST_BUTTON);

  views::View* view = new views::View();
  views::GridLayout* layout = new views::GridLayout(view);
  view->SetLayoutManager(layout);

  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 0,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->LinkColumnSizes(0, 2, -1);

  const int kButtonHeight = 40;
  layout->StartRow(0, 0);
  layout->AddView(users_button_view_, 1, 1,
                  views::GridLayout::FILL, views::GridLayout::FILL,
                  0, kButtonHeight);
  layout->AddView(new views::Separator(views::Separator::VERTICAL));
  layout->AddView(guest_button_view_, 1, 1,
                  views::GridLayout::FILL, views::GridLayout::FILL,
                  0, kButtonHeight);

  return view;
}
