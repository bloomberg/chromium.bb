// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/before_translate_infobar.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/translate/options_menu_model.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "chrome/browser/ui/views/infobars/infobar_text_button.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/image_view.h"
#include "views/controls/menu/menu_2.h"

BeforeTranslateInfoBar::BeforeTranslateInfoBar(
    TranslateInfoBarDelegate* delegate)
    : TranslateInfoBarBase(delegate),
      never_translate_button_(NULL),
      always_translate_button_(NULL),
      languages_menu_model_(delegate, LanguagesMenuModel::ORIGINAL),
      options_menu_model_(delegate) {
  size_t offset = 0;
  string16 text =
      l10n_util::GetStringFUTF16(IDS_TRANSLATE_INFOBAR_BEFORE_MESSAGE,
                                 string16(), &offset);

  label_1_ = CreateLabel(text.substr(0, offset));
  AddChildView(label_1_);

  label_2_ = CreateLabel(text.substr(offset));
  AddChildView(label_2_);

  accept_button_ =
    InfoBarTextButton::CreateWithMessageID(this,
                                           IDS_TRANSLATE_INFOBAR_ACCEPT);
  AddChildView(accept_button_);

  deny_button_ =
    InfoBarTextButton::CreateWithMessageID(this,
                                           IDS_TRANSLATE_INFOBAR_DENY);
  AddChildView(deny_button_);

  language_menu_button_ = CreateMenuButton(string16(), true, this);
  AddChildView(language_menu_button_);

  options_menu_button_ =
      CreateMenuButton(l10n_util::GetStringUTF16(IDS_TRANSLATE_INFOBAR_OPTIONS),
                       false, this);
  AddChildView(options_menu_button_);

  if (delegate->ShouldShowNeverTranslateButton()) {
    const string16& language = delegate->GetLanguageDisplayableNameAt(
        delegate->original_language_index());
    never_translate_button_ = InfoBarTextButton::CreateWithMessageIDAndParam(
        this, IDS_TRANSLATE_INFOBAR_NEVER_TRANSLATE, language);
    AddChildView(never_translate_button_);
  }

  if (delegate->ShouldShowAlwaysTranslateButton()) {
    const string16& language = delegate->GetLanguageDisplayableNameAt(
        delegate->original_language_index());
    always_translate_button_ = InfoBarTextButton::CreateWithMessageIDAndParam(
        this, IDS_TRANSLATE_INFOBAR_ALWAYS_TRANSLATE, language);
    AddChildView(always_translate_button_);
  }

  UpdateOriginalButtonText();
}

BeforeTranslateInfoBar::~BeforeTranslateInfoBar() {
}

// Overridden from views::View:
void BeforeTranslateInfoBar::Layout() {
  // Layout the icon and close button.
  TranslateInfoBarBase::Layout();

  // Layout the options menu button on right of bar.
  int available_width = InfoBarView::GetAvailableWidth();
  gfx::Size pref_size = options_menu_button_->GetPreferredSize();
  options_menu_button_->SetBounds(available_width - pref_size.width(),
      OffsetY(this, pref_size), pref_size.width(), pref_size.height());

  pref_size = label_1_->GetPreferredSize();
  label_1_->SetBounds(icon_->bounds().right() + InfoBarView::kIconLabelSpacing,
                      InfoBarView::OffsetY(this, pref_size), pref_size.width(),
                      pref_size.height());

  pref_size = language_menu_button_->GetPreferredSize();
  language_menu_button_->SetBounds(label_1_->bounds().right() +
      InfoBarView::kButtonInLabelSpacing, OffsetY(this, pref_size),
      pref_size.width(), pref_size.height());

  pref_size = label_2_->GetPreferredSize();
  label_2_->SetBounds(language_menu_button_->bounds().right() +
      InfoBarView::kButtonInLabelSpacing, InfoBarView::OffsetY(this, pref_size),
      pref_size.width(), pref_size.height());

  pref_size = accept_button_->GetPreferredSize();
  accept_button_->SetBounds(
      label_2_->bounds().right() + InfoBarView::kEndOfLabelSpacing,
      OffsetY(this, pref_size), pref_size.width(), pref_size.height());

  pref_size = deny_button_->GetPreferredSize();
  deny_button_->SetBounds(
        accept_button_->bounds().right() + InfoBarView::kButtonButtonSpacing,
        OffsetY(this, pref_size), pref_size.width(), pref_size.height());

  if (never_translate_button_) {
    pref_size = never_translate_button_->GetPreferredSize();
    never_translate_button_->SetBounds(
          deny_button_->bounds().right() + InfoBarView::kButtonButtonSpacing,
          OffsetY(this, pref_size), pref_size.width(), pref_size.height());
  }
  if (always_translate_button_) {
    DCHECK(!never_translate_button_);
    pref_size = always_translate_button_->GetPreferredSize();
    always_translate_button_->SetBounds(
          deny_button_->bounds().right() + InfoBarView::kButtonButtonSpacing,
          OffsetY(this, pref_size), pref_size.width(), pref_size.height());
  }
}

void BeforeTranslateInfoBar::ButtonPressed(views::Button* sender,
                                           const views::Event& event) {
  if (sender == accept_button_) {
    GetDelegate()->Translate();
  } else if (sender == deny_button_) {
    RemoveInfoBar();
    GetDelegate()->TranslationDeclined();
  } else if (sender == never_translate_button_) {
    GetDelegate()->NeverTranslatePageLanguage();
  } else if (sender == always_translate_button_) {
    GetDelegate()->AlwaysTranslatePageLanguage();
  } else {
    TranslateInfoBarBase::ButtonPressed(sender, event);
  }
}

void BeforeTranslateInfoBar::OriginalLanguageChanged() {
  UpdateOriginalButtonText();
}

void BeforeTranslateInfoBar::TargetLanguageChanged() {
  NOTREACHED();
}

void BeforeTranslateInfoBar::RunMenu(views::View* source,
                                     const gfx::Point& pt) {
  if (source == language_menu_button_) {
    if (!languages_menu_.get())
      languages_menu_.reset(new views::Menu2(&languages_menu_model_));
    languages_menu_->RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
  } else if (source == options_menu_button_) {
    if (!options_menu_.get())
      options_menu_.reset(new views::Menu2(&options_menu_model_));
    options_menu_->RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
  } else {
    NOTREACHED();
  }
}

void BeforeTranslateInfoBar::UpdateOriginalButtonText() {
  string16 language = GetDelegate()->GetLanguageDisplayableNameAt(
      GetDelegate()->original_language_index());
  language_menu_button_->SetText(UTF16ToWideHack(language));
  // The following line is necessary for the preferred size to be recomputed.
  language_menu_button_->ClearMaxTextSize();
  // The button may have to grow to show the new text.
  Layout();
  SchedulePaint();
}
