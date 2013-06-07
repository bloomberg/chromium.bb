// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/after_translate_infobar.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"

AfterTranslateInfoBar::AfterTranslateInfoBar(
    InfoBarService* owner,
    TranslateInfoBarDelegate* delegate)
    : TranslateInfoBarBase(owner, delegate),
      label_1_(NULL),
      label_2_(NULL),
      label_3_(NULL),
      original_language_menu_button_(NULL),
      target_language_menu_button_(NULL),
      revert_button_(NULL),
      options_menu_button_(NULL),
      options_menu_model_(delegate),
      swapped_language_buttons_(false) {
  autodetermined_source_language_ =
      delegate->original_language_index() == TranslateInfoBarDelegate::kNoIndex;
}

AfterTranslateInfoBar::~AfterTranslateInfoBar() {
}

void AfterTranslateInfoBar::Layout() {
  TranslateInfoBarBase::Layout();

  int available_width = std::max(0, EndX() - StartX() - ContentMinimumWidth());
  gfx::Size label_1_size = label_1_->GetPreferredSize();
  label_1_->SetBounds(StartX(), OffsetY(label_1_size),
      std::min(label_1_size.width(), available_width), label_1_size.height());
  available_width = std::max(0, available_width - label_1_size.width());

  views::MenuButton* first_button = original_language_menu_button_;
  views::MenuButton* second_button = target_language_menu_button_;
  if (swapped_language_buttons_ || autodetermined_source_language_)
    std::swap(first_button, second_button);
  gfx::Size first_button_size = first_button->GetPreferredSize();
  first_button->SetBounds(label_1_->bounds().right() + kButtonInLabelSpacing,
      OffsetY(first_button_size), first_button_size.width(),
      first_button_size.height());

  gfx::Size label_2_size = label_2_->GetPreferredSize();
  label_2_->SetBounds(first_button->bounds().right() + kButtonInLabelSpacing,
      OffsetY(label_2_size), std::min(label_2_size.width(), available_width),
      label_2_size.height());
  available_width = std::max(0, available_width - label_2_size.width());

  if (!autodetermined_source_language_) {
    gfx::Size second_button_size = second_button->GetPreferredSize();
    second_button->SetBounds(label_2_->bounds().right() + kButtonInLabelSpacing,
        OffsetY(second_button_size), second_button_size.width(),
        second_button_size.height());

    gfx::Size label_3_size = label_3_->GetPreferredSize();
    label_3_->SetBounds(second_button->bounds().right() + kButtonInLabelSpacing,
        OffsetY(label_3_size), std::min(label_3_size.width(), available_width),
        label_3_size.height());
  }

  gfx::Size revert_button_size = revert_button_->GetPreferredSize();
  revert_button_->SetBounds(
      (label_3_ ? label_3_ : label_2_)->bounds().right() +
          kButtonInLabelSpacing,
      OffsetY(revert_button_size),
      revert_button_size.width(),
      revert_button_size.height());

  gfx::Size options_size = options_menu_button_->GetPreferredSize();
  options_menu_button_->SetBounds(EndX() - options_size.width(),
      OffsetY(options_size), options_size.width(), options_size.height());
}

void AfterTranslateInfoBar::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (!details.is_add || (details.child != this) || (label_1_ != NULL)) {
    TranslateInfoBarBase::ViewHierarchyChanged(details);
    return;
  }

  std::vector<string16> strings;
  TranslateInfoBarDelegate::GetAfterTranslateStrings(
      &strings, &swapped_language_buttons_, autodetermined_source_language_);
  DCHECK_EQ(autodetermined_source_language_ ? 2U : 3U, strings.size());

  label_1_ = CreateLabel(strings[0]);
  AddChildView(label_1_);

  TranslateInfoBarDelegate* delegate = GetDelegate();
  original_language_menu_button_ = CreateMenuButton(string16(), this);
  original_language_menu_model_.reset(new TranslateLanguageMenuModel(
      TranslateLanguageMenuModel::ORIGINAL, delegate, this,
      original_language_menu_button_, true));
  target_language_menu_button_ = CreateMenuButton(string16(), this);
  target_language_menu_model_.reset(new TranslateLanguageMenuModel(
      TranslateLanguageMenuModel::TARGET, delegate, this,
      target_language_menu_button_, true));
  AddChildView((swapped_language_buttons_ || autodetermined_source_language_) ?
      target_language_menu_button_ : original_language_menu_button_);

  label_2_ = CreateLabel(strings[1]);
  AddChildView(label_2_);

  if (!autodetermined_source_language_) {
    AddChildView(swapped_language_buttons_ ?
        original_language_menu_button_ : target_language_menu_button_);

    label_3_ = CreateLabel(strings[2]);
    AddChildView(label_3_);
  }

  revert_button_ = CreateLabelButton(this,
      l10n_util::GetStringUTF16(IDS_TRANSLATE_INFOBAR_REVERT), false);
  AddChildView(revert_button_);

  options_menu_button_ = CreateMenuButton(
      l10n_util::GetStringUTF16(IDS_TRANSLATE_INFOBAR_OPTIONS), this);
  AddChildView(options_menu_button_);

  // This must happen after adding all other children so InfoBarView can ensure
  // the close button is the last child.
  TranslateInfoBarBase::ViewHierarchyChanged(details);

  // These must happen after adding all children because they trigger layout,
  // which assumes that particular children (e.g. the close button) have already
  // been added.
  if (!autodetermined_source_language_) {
    UpdateLanguageButtonText(original_language_menu_button_,
        delegate->language_name_at(delegate->original_language_index()));
  }
  UpdateLanguageButtonText(target_language_menu_button_,
      delegate->language_name_at(delegate->target_language_index()));
}

void AfterTranslateInfoBar::ButtonPressed(views::Button* sender,
                                          const ui::Event& event) {
  if (!owner())
    return;  // We're closing; don't call anything, it might access the owner.
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

void AfterTranslateInfoBar::OnMenuButtonClicked(views::View* source,
                                                const gfx::Point& point) {
  if (!owner())
    return;  // We're closing; don't call anything, it might access the owner.
  if (source == original_language_menu_button_) {
    RunMenuAt(original_language_menu_model_.get(),
              original_language_menu_button_, views::MenuItemView::TOPLEFT);
  } else if (source == target_language_menu_button_) {
    RunMenuAt(target_language_menu_model_.get(), target_language_menu_button_,
              views::MenuItemView::TOPLEFT);
  } else {
    DCHECK_EQ(options_menu_button_, source);
    RunMenuAt(&options_menu_model_, options_menu_button_,
              views::MenuItemView::TOPRIGHT);
  }
}
