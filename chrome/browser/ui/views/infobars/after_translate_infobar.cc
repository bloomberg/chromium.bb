// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/after_translate_infobar.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/translate/options_menu_model.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "chrome/browser/ui/views/infobars/infobar_button_border.h"
#include "chrome/browser/ui/views/infobars/infobar_text_button.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/button/text_button.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/controls/menu/menu_2.h"

AfterTranslateInfoBar::AfterTranslateInfoBar(
    TranslateInfoBarDelegate* delegate)
    : TranslateInfoBarBase(delegate),
      original_language_menu_model_(delegate, LanguagesMenuModel::ORIGINAL),
      target_language_menu_model_(delegate, LanguagesMenuModel::TARGET),
      options_menu_model_(delegate),
      swapped_language_buttons_(false) {
  std::vector<string16> strings;
  TranslateInfoBarDelegate::GetAfterTranslateStrings(
      &strings, &swapped_language_buttons_);
  DCHECK(strings.size() == 3U);

  label_1_ = CreateLabel(strings[0]);
  AddChildView(label_1_);

  label_2_ = CreateLabel(strings[1]);
  AddChildView(label_2_);

  label_3_ = CreateLabel(strings[2]);
  AddChildView(label_3_);

  original_language_menu_button_ = CreateMenuButton(string16(), true, this);
  AddChildView(original_language_menu_button_);

  target_language_menu_button_ = CreateMenuButton(string16(), true, this);
  AddChildView(target_language_menu_button_);

  options_menu_button_ =
      CreateMenuButton(l10n_util::GetStringUTF16(IDS_TRANSLATE_INFOBAR_OPTIONS),
                       false, this);
  AddChildView(options_menu_button_);

  revert_button_ = InfoBarTextButton::Create(this,
      l10n_util::GetStringUTF16(IDS_TRANSLATE_INFOBAR_REVERT));
  AddChildView(revert_button_);

  UpdateLanguageButtonText(LanguagesMenuModel::ORIGINAL);
  UpdateLanguageButtonText(LanguagesMenuModel::TARGET);
}

AfterTranslateInfoBar::~AfterTranslateInfoBar() {
}

// Overridden from views::View:
void AfterTranslateInfoBar::Layout() {
  // Layout the icon and close button.
  TranslateInfoBarBase::Layout();

  // Layout the options menu button on right of bar.
  int available_width = InfoBar::GetAvailableWidth();
  gfx::Size pref_size = options_menu_button_->GetPreferredSize();
  options_menu_button_->SetBounds(available_width - pref_size.width(),
      OffsetY(this, pref_size), pref_size.width(), pref_size.height());

  views::MenuButton* left_button = swapped_language_buttons_ ?
      target_language_menu_button_ : original_language_menu_button_;
  views::MenuButton* right_button = swapped_language_buttons_ ?
      original_language_menu_button_ : target_language_menu_button_;

  pref_size = label_1_->GetPreferredSize();
  label_1_->SetBounds(icon_->bounds().right() + InfoBar::kIconLabelSpacing,
      InfoBar::OffsetY(this, pref_size), pref_size.width(), pref_size.height());

  pref_size = left_button->GetPreferredSize();
  left_button->SetBounds(label_1_->bounds().right() +
      InfoBar::kButtonInLabelSpacing, OffsetY(this, pref_size),
      pref_size.width(), pref_size.height());

  pref_size = label_2_->GetPreferredSize();
  label_2_->SetBounds(left_button->bounds().right() +
      InfoBar::kButtonInLabelSpacing, InfoBar::OffsetY(this, pref_size),
      pref_size.width(), pref_size.height());

  pref_size = right_button->GetPreferredSize();
  right_button->SetBounds(label_2_->bounds().right() +
      InfoBar::kButtonInLabelSpacing, OffsetY(this, pref_size),
      pref_size.width(), pref_size.height());

  pref_size = label_3_->GetPreferredSize();
  label_3_->SetBounds(right_button->bounds().right() +
      InfoBar::kButtonInLabelSpacing, InfoBar::OffsetY(this, pref_size),
      pref_size.width(), pref_size.height());

  pref_size = revert_button_->GetPreferredSize();
  revert_button_->SetBounds(label_3_->bounds().right() +
      InfoBar::kButtonInLabelSpacing, InfoBar::OffsetY(this, pref_size),
      pref_size.width(), pref_size.height());
}

void AfterTranslateInfoBar::OriginalLanguageChanged() {
  UpdateLanguageButtonText(LanguagesMenuModel::ORIGINAL);
}

void AfterTranslateInfoBar::TargetLanguageChanged() {
  UpdateLanguageButtonText(LanguagesMenuModel::TARGET);
}

void AfterTranslateInfoBar::ButtonPressed(views::Button* sender,
                                          const views::Event& event) {
  if (sender == revert_button_) {
    GetDelegate()->RevertTranslation();
    return;
  }
  TranslateInfoBarBase::ButtonPressed(sender, event);
}

void AfterTranslateInfoBar::RunMenu(views::View* source,
                                    const gfx::Point& pt) {
  if (source == original_language_menu_button_) {
    if (!original_language_menu_.get()) {
      original_language_menu_.reset(
          new views::Menu2(&original_language_menu_model_));
    }
    original_language_menu_->RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
  } else if (source == target_language_menu_button_) {
      if (!target_language_menu_.get()) {
        target_language_menu_.reset(
            new views::Menu2(&target_language_menu_model_));
      }
      target_language_menu_->RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
  } else if (source == options_menu_button_) {
    if (!options_menu_.get())
      options_menu_.reset(new views::Menu2(&options_menu_model_));
    options_menu_->RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
  } else {
    NOTREACHED();
  }
}

void AfterTranslateInfoBar::UpdateLanguageButtonText(
    LanguagesMenuModel::LanguageType language_type) {
  int language_index;
  views::MenuButton* language_button;
  if (language_type == LanguagesMenuModel::ORIGINAL) {
    language_index = GetDelegate()->original_language_index();
    language_button = original_language_menu_button_;
  } else {
    language_index = GetDelegate()->target_language_index();
    language_button = target_language_menu_button_;
  }
  string16 language =
      GetDelegate()->GetLanguageDisplayableNameAt(language_index);
  language_button->SetText(UTF16ToWideHack(language));
  // The following line is necessary for the preferred size to be recomputed.
  language_button->ClearMaxTextSize();
  // The button may have to grow to show the new text.
  Layout();
  SchedulePaint();
}
