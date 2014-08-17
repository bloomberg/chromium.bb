// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_password_item_view.h"

#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

namespace {

enum FieldType { USERNAME_FIELD, PASSWORD_FIELD };

// Upper limit on the size of the username and password fields.
const int kUsernameFieldSize = 30;
const int kPasswordFieldSize = 22;

// Returns the width of |type| field.
int GetFieldWidth(FieldType type) {
  return ui::ResourceBundle::GetSharedInstance()
      .GetFontList(ui::ResourceBundle::SmallFont)
      .GetExpectedTextWidth(type == USERNAME_FIELD ? kUsernameFieldSize
                                                   : kPasswordFieldSize);
}

int FirstFieldWidth() {
  return std::max(
      GetFieldWidth(USERNAME_FIELD),
      views::Label(l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_DELETED))
          .GetPreferredSize()
          .width());
}

int SecondFieldWidth() {
  return std::max(
      GetFieldWidth(PASSWORD_FIELD),
      views::Label(l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_UNDO))
          .GetPreferredSize()
          .width());
}

enum ColumnSets { TWO_COLUMN_SET = 0, THREE_COLUMN_SET };

void BuildColumnSet(views::GridLayout* layout, int column_set_id) {
  views::ColumnSet* column_set = layout->AddColumnSet(column_set_id);

  // The username/"Deleted!" field.
  column_set->AddPaddingColumn(0, views::kItemLabelSpacing);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        0,
                        views::GridLayout::FIXED,
                        FirstFieldWidth(),
                        FirstFieldWidth());

  // The password/"Undo!" field.
  column_set->AddPaddingColumn(0, views::kItemLabelSpacing);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::USE_PREF,
                        SecondFieldWidth(),
                        SecondFieldWidth());

  // If we're in manage-mode, we need another column for the delete button.
  if (column_set_id == THREE_COLUMN_SET) {
    column_set->AddPaddingColumn(0, views::kItemLabelSpacing);
    column_set->AddColumn(views::GridLayout::TRAILING,
                          views::GridLayout::FILL,
                          0,
                          views::GridLayout::USE_PREF,
                          0,
                          0);
  }
  column_set->AddPaddingColumn(0, views::kItemLabelSpacing);
}

views::Label* GenerateUsernameLabel(const autofill::PasswordForm& form) {
  views::Label* label = new views::Label(form.username_value);
  label->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return label;
}

views::Label* GeneratePasswordLabel(const autofill::PasswordForm& form) {
  views::Label* label = new views::Label(form.password_value);
  label->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetObscured(true);
  return label;
}

}  // namespace

// Pending View
ManagePasswordItemView::PendingView::PendingView(
    ManagePasswordItemView* parent) {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  BuildColumnSet(layout, TWO_COLUMN_SET);
  layout->StartRowWithPadding(
      0, TWO_COLUMN_SET, 0, views::kRelatedControlVerticalSpacing);
  layout->AddView(GenerateUsernameLabel(parent->password_form_));
  layout->AddView(GeneratePasswordLabel(parent->password_form_));
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
}

ManagePasswordItemView::PendingView::~PendingView() {
}

// Manage View
ManagePasswordItemView::ManageView::ManageView(ManagePasswordItemView* parent)
    : parent_(parent) {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  delete_button_ = new views::ImageButton(this);
  delete_button_->SetImage(views::ImageButton::STATE_NORMAL,
                           rb->GetImageNamed(IDR_CLOSE_2).ToImageSkia());
  delete_button_->SetImage(views::ImageButton::STATE_HOVERED,
                           rb->GetImageNamed(IDR_CLOSE_2_H).ToImageSkia());
  delete_button_->SetImage(views::ImageButton::STATE_PRESSED,
                           rb->GetImageNamed(IDR_CLOSE_2_P).ToImageSkia());

  BuildColumnSet(layout, THREE_COLUMN_SET);
  layout->StartRowWithPadding(
      0, THREE_COLUMN_SET, 0, views::kRelatedControlVerticalSpacing);
  layout->AddView(GenerateUsernameLabel(parent->password_form_));
  layout->AddView(GeneratePasswordLabel(parent->password_form_));
  layout->AddView(delete_button_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
}

void ManagePasswordItemView::ManageView::ButtonPressed(views::Button* sender,
                                                       const ui::Event& event) {
  DCHECK_EQ(delete_button_, sender);
  parent_->NotifyClickedDelete();
}

ManagePasswordItemView::ManageView::~ManageView() {
}

// Undo View
ManagePasswordItemView::UndoView::UndoView(ManagePasswordItemView* parent)
    : parent_(parent) {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  views::Label* text =
      new views::Label(l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_DELETED));
  text->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  text->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));

  undo_link_ =
      new views::Link(l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_UNDO));
  undo_link_->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  undo_link_->set_listener(this);
  undo_link_->SetUnderline(false);
  undo_link_->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));

  BuildColumnSet(layout, TWO_COLUMN_SET);
  layout->StartRowWithPadding(
      0, TWO_COLUMN_SET, 0, views::kRelatedControlVerticalSpacing);
  layout->AddView(text);
  layout->AddView(undo_link_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
}

void ManagePasswordItemView::UndoView::LinkClicked(views::Link* sender,
                                                   int event_flags) {
  DCHECK_EQ(undo_link_, sender);
  parent_->NotifyClickedUndo();
}

ManagePasswordItemView::UndoView::~UndoView() {
}

// ManagePasswordItemView
ManagePasswordItemView::ManagePasswordItemView(
    ManagePasswordsBubbleModel* manage_passwords_bubble_model,
    autofill::PasswordForm password_form,
    password_manager::ui::PasswordItemPosition position)
    : model_(manage_passwords_bubble_model),
      password_form_(password_form),
      delete_password_(false) {
  views::FillLayout* layout = new views::FillLayout();
  SetLayoutManager(layout);

  // When a password is displayed as the first item in a list, it has borders
  // on both the top and bottom. When it's in the middle of a list, or at the
  // end, it has a border only on the bottom.
  SetBorder(views::Border::CreateSolidSidedBorder(
      position == password_manager::ui::FIRST_ITEM ? 1 : 0,
      0,
      1,
      0,
      GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_EnabledMenuButtonBorderColor)));

  if (password_manager::ui::IsPendingState(model_->state())) {
    AddChildView(new PendingView(this));
  } else {
    AddChildView(new ManageView(this));
  }
  GetLayoutManager()->Layout(this);
}

ManagePasswordItemView::~ManagePasswordItemView() {
}

void ManagePasswordItemView::Refresh() {
  DCHECK(!password_manager::ui::IsPendingState(model_->state()));

  RemoveAllChildViews(true);
  if (delete_password_)
    AddChildView(new UndoView(this));
  else
    AddChildView(new ManageView(this));
  GetLayoutManager()->Layout(this);

  // After the view is consistent, notify the model that the password needs to
  // be updated (either removed or put back into the store, as appropriate.
  model_->OnPasswordAction(password_form_,
                           delete_password_
                               ? ManagePasswordsBubbleModel::REMOVE_PASSWORD
                               : ManagePasswordsBubbleModel::ADD_PASSWORD);
}

void ManagePasswordItemView::NotifyClickedDelete() {
  delete_password_ = true;
  Refresh();
}

void ManagePasswordItemView::NotifyClickedUndo() {
  delete_password_ = false;
  Refresh();
}
