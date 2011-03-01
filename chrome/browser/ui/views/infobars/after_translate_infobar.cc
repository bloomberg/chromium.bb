// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/after_translate_infobar.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/label.h"
#include "views/controls/menu/menu_2.h"

AfterTranslateInfoBar::AfterTranslateInfoBar(
    TranslateInfoBarDelegate* delegate)
    : TranslateInfoBarBase(delegate),
      label_1_(NULL),
      label_2_(NULL),
      label_3_(NULL),
      original_language_menu_button_(NULL),
      target_language_menu_button_(NULL),
      revert_button_(NULL),
      options_menu_button_(NULL),
      original_language_menu_model_(delegate, LanguagesMenuModel::ORIGINAL),
      target_language_menu_model_(delegate, LanguagesMenuModel::TARGET),
      options_menu_model_(delegate),
      swapped_language_buttons_(false) {
}

AfterTranslateInfoBar::~AfterTranslateInfoBar() {
}

void AfterTranslateInfoBar::Layout() {
  TranslateInfoBarBase::Layout();

  int available_width = std::max(0, EndX() - StartX() - ContentMinimumWidth());
  gfx::Size label_1_size = label_1_->GetPreferredSize();
  label_1_->SetBounds(StartX(), OffsetY(this, label_1_size),
      std::min(label_1_size.width(), available_width), label_1_size.height());
  available_width = std::max(0, available_width - label_1_size.width());

  views::MenuButton* first_button = original_language_menu_button_;
  views::MenuButton* second_button = target_language_menu_button_;
  if (swapped_language_buttons_)
    std::swap(first_button, second_button);
  gfx::Size first_button_size = first_button->GetPreferredSize();
  first_button->SetBounds(label_1_->bounds().right() + kButtonInLabelSpacing,
      OffsetY(this, first_button_size), first_button_size.width(),
      first_button_size.height());

  gfx::Size label_2_size = label_2_->GetPreferredSize();
  label_2_->SetBounds(first_button->bounds().right() + kButtonInLabelSpacing,
      OffsetY(this, label_2_size),
      std::min(label_2_size.width(), available_width), label_2_size.height());
  available_width = std::max(0, available_width - label_2_size.width());

  gfx::Size second_button_size = second_button->GetPreferredSize();
  second_button->SetBounds(label_2_->bounds().right() + kButtonInLabelSpacing,
      OffsetY(this, second_button_size), second_button_size.width(),
      second_button_size.height());

  gfx::Size label_3_size = label_3_->GetPreferredSize();
  label_3_->SetBounds(second_button->bounds().right() + kButtonInLabelSpacing,
      OffsetY(this, label_3_size),
      std::min(label_3_size.width(), available_width), label_3_size.height());

  gfx::Size revert_button_size = revert_button_->GetPreferredSize();
  revert_button_->SetBounds(label_3_->bounds().right() + kButtonInLabelSpacing,
      OffsetY(this, revert_button_size),
      revert_button_size.width(), revert_button_size.height());

  gfx::Size options_size = options_menu_button_->GetPreferredSize();
  options_menu_button_->SetBounds(EndX() - options_size.width(),
      OffsetY(this, options_size), options_size.width(), options_size.height());
}

void AfterTranslateInfoBar::ViewHierarchyChanged(bool is_add,
                                                 View* parent,
                                                 View* child) {
  if (!is_add || (child != this) || (label_1_ != NULL)) {
    TranslateInfoBarBase::ViewHierarchyChanged(is_add, parent, child);
    return;
  }

  std::vector<string16> strings;
  GetDelegate()->GetAfterTranslateStrings(&strings, &swapped_language_buttons_);
  DCHECK_EQ(3U, strings.size());

  label_1_ = CreateLabel(strings[0]);
  AddChildView(label_1_);

  original_language_menu_button_ = CreateMenuButton(string16(), true, this);
  target_language_menu_button_ = CreateMenuButton(string16(), true, this);
  AddChildView(swapped_language_buttons_ ?
      target_language_menu_button_ : original_language_menu_button_);

  label_2_ = CreateLabel(strings[1]);
  AddChildView(label_2_);

  AddChildView(swapped_language_buttons_ ?
      original_language_menu_button_ : target_language_menu_button_);

  label_3_ = CreateLabel(strings[2]);
  AddChildView(label_3_);

  revert_button_ = CreateTextButton(this,
      l10n_util::GetStringUTF16(IDS_TRANSLATE_INFOBAR_REVERT), false);
  AddChildView(revert_button_);

  options_menu_button_ = CreateMenuButton(
      l10n_util::GetStringUTF16(IDS_TRANSLATE_INFOBAR_OPTIONS), false, this);
  AddChildView(options_menu_button_);

  // This must happen after adding all other children so InfoBarView can ensure
  // the close button is the last child.
  TranslateInfoBarBase::ViewHierarchyChanged(is_add, parent, child);

  // These must happen after adding all children because they trigger layout,
  // which assumes that particular children (e.g. the close button) have already
  // been added.
  OriginalLanguageChanged();
  TargetLanguageChanged();
}

void AfterTranslateInfoBar::ButtonPressed(views::Button* sender,
                                          const views::Event& event) {
  if (sender == revert_button_)
    GetDelegate()->RevertTranslation();
  else
    TranslateInfoBarBase::ButtonPressed(sender, event);
}

int AfterTranslateInfoBar::ContentMinimumWidth() const {
  return
      (kButtonInLabelSpacing +
           original_language_menu_button_->GetPreferredSize().width() +
           kButtonInLabelSpacing) +
      (kButtonInLabelSpacing +
           target_language_menu_button_->GetPreferredSize().width() +
           kButtonInLabelSpacing) +
      (kButtonInLabelSpacing + revert_button_->GetPreferredSize().width()) +
      (kEndOfLabelSpacing + options_menu_button_->GetPreferredSize().width());
}

void AfterTranslateInfoBar::OriginalLanguageChanged() {
  UpdateLanguageButtonText(original_language_menu_button_,
                           LanguagesMenuModel::ORIGINAL);
}

void AfterTranslateInfoBar::TargetLanguageChanged() {
  UpdateLanguageButtonText(target_language_menu_button_,
                           LanguagesMenuModel::TARGET);
}

void AfterTranslateInfoBar::RunMenu(View* source, const gfx::Point& pt) {
  if (source == original_language_menu_button_) {
    original_language_menu_->RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
  } else if (source == target_language_menu_button_) {
    target_language_menu_->RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
  } else {
    DCHECK_EQ(options_menu_button_, source);
    options_menu_->RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
  }
}
