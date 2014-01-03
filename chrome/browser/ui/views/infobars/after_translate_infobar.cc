// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/after_translate_infobar.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/translate/options_menu_model.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "chrome/browser/ui/views/infobars/translate_language_menu_model.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"

AfterTranslateInfoBar::AfterTranslateInfoBar(
    scoped_ptr<TranslateInfoBarDelegate> delegate)
    : TranslateInfoBarBase(delegate.Pass()),
      label_1_(NULL),
      label_2_(NULL),
      label_3_(NULL),
      original_language_menu_button_(NULL),
      target_language_menu_button_(NULL),
      revert_button_(NULL),
      options_menu_button_(NULL),
      swapped_language_buttons_(false) {
  autodetermined_source_language_ =
      GetDelegate()->original_language_index() ==
      TranslateInfoBarDelegate::kNoIndex;
}

AfterTranslateInfoBar::~AfterTranslateInfoBar() {
}

void AfterTranslateInfoBar::Layout() {
  TranslateInfoBarBase::Layout();

  int x = StartX();
  Labels labels;
  labels.push_back(label_1_);
  labels.push_back(label_2_);
  labels.push_back(label_3_);
  AssignWidths(&labels, std::max(0, EndX() - x - NonLabelWidth()));

  label_1_->SetPosition(gfx::Point(x, OffsetY(label_1_)));
  if (!label_1_->text().empty())
     x = label_1_->bounds().right() + kButtonInLabelSpacing;

  views::MenuButton* first_button, * second_button;
  GetButtons(&first_button, &second_button);
  first_button->SetPosition(gfx::Point(x, OffsetY(first_button)));
  x = first_button->bounds().right();

  label_2_->SetPosition(
      gfx::Point(x + kButtonInLabelSpacing, OffsetY(label_2_)));
  if (!label_2_->text().empty())
    x = label_2_->bounds().right();

  if (!autodetermined_source_language_) {
    x += label_2_->text().empty() ?
        kButtonButtonSpacing : kButtonInLabelSpacing;
    second_button->SetPosition(gfx::Point(x, OffsetY(second_button)));
    x = second_button->bounds().right();

    label_3_->SetPosition(
        gfx::Point(x + kButtonInLabelSpacing, OffsetY(label_3_)));
    if (!label_3_->text().empty())
      x = label_3_->bounds().right();
  }

  revert_button_->SetPosition(
      gfx::Point(x + kEndOfLabelSpacing, OffsetY(revert_button_)));

  options_menu_button_->SetPosition(gfx::Point(
      EndX() - options_menu_button_->width(), OffsetY(options_menu_button_)));
}

void AfterTranslateInfoBar::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (!details.is_add || (details.child != this) || (label_1_ != NULL)) {
    TranslateInfoBarBase::ViewHierarchyChanged(details);
    return;
  }

  std::vector<base::string16> strings;
  TranslateInfoBarDelegate::GetAfterTranslateStrings(
      &strings, &swapped_language_buttons_, autodetermined_source_language_);
  DCHECK_EQ(autodetermined_source_language_ ? 2U : 3U, strings.size());

  label_1_ = CreateLabel(strings[0]);
  AddChildView(label_1_);

  TranslateInfoBarDelegate* delegate = GetDelegate();
  original_language_menu_button_ = CreateMenuButton(base::string16(), this);
  original_language_menu_model_.reset(new TranslateLanguageMenuModel(
      TranslateLanguageMenuModel::ORIGINAL, delegate, this,
      original_language_menu_button_, true));
  target_language_menu_button_ = CreateMenuButton(base::string16(), this);
  target_language_menu_model_.reset(new TranslateLanguageMenuModel(
      TranslateLanguageMenuModel::TARGET, delegate, this,
      target_language_menu_button_, true));

  views::MenuButton* first_button, * second_button;
  GetButtons(&first_button, &second_button);
  AddChildView(first_button);

  label_2_ = CreateLabel(strings[1]);
  AddChildView(label_2_);

  // These views may not always be shown, but adding them unconditionally
  // prevents leaks and reduces NULL-checking elsewhere.
  AddChildView(second_button);
  if (autodetermined_source_language_)
    second_button->SetVisible(false);
  label_3_ = CreateLabel(autodetermined_source_language_ ?
      base::string16() : strings[2]);
  AddChildView(label_3_);

  revert_button_ = CreateLabelButton(this,
      l10n_util::GetStringUTF16(IDS_TRANSLATE_INFOBAR_REVERT), false);
  AddChildView(revert_button_);

  options_menu_button_ = CreateMenuButton(
      l10n_util::GetStringUTF16(IDS_TRANSLATE_INFOBAR_OPTIONS), this);
  options_menu_model_.reset(new OptionsMenuModel(delegate));
  AddChildView(options_menu_button_);

  // This must happen after adding all other children so InfoBarView can ensure
  // the close button is the last child.
  TranslateInfoBarBase::ViewHierarchyChanged(details);

  // These must happen after adding all children because they trigger layout,
  // which assumes that particular children (e.g. the close button) have already
  // been added.
  UpdateLanguageButtonText(original_language_menu_button_,
      delegate->language_name_at(delegate->original_language_index()));
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

int AfterTranslateInfoBar::ContentMinimumWidth() {
  return label_1_->GetMinimumSize().width() +
      label_2_->GetMinimumSize().width() + label_3_->GetMinimumSize().width() +
      NonLabelWidth();
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
    RunMenuAt(options_menu_model_.get(), options_menu_button_,
              views::MenuItemView::TOPRIGHT);
  }
}

void AfterTranslateInfoBar::GetButtons(
    views::MenuButton** first_button,
    views::MenuButton** second_button) const {
  *first_button = original_language_menu_button_;
  *second_button = target_language_menu_button_;
  if (swapped_language_buttons_ || autodetermined_source_language_)
    std::swap(*first_button, *second_button);
}

int AfterTranslateInfoBar::NonLabelWidth() const {
  views::MenuButton* first_button, *second_button;
  GetButtons(&first_button, &second_button);
  int width = (label_1_->text().empty() ? 0 : kButtonInLabelSpacing) +
      first_button->width() +
      (label_2_->text().empty() ? 0 : kButtonInLabelSpacing);
  if (!autodetermined_source_language_) {
    width +=
        (label_2_->text().empty() ?
            kButtonButtonSpacing : kButtonInLabelSpacing) +
        second_button->width() +
        (label_3_->text().empty() ? 0 : kButtonInLabelSpacing);
  }
  return width + kEndOfLabelSpacing + revert_button_->width() +
      kEndOfLabelSpacing + options_menu_button_->width();
}
