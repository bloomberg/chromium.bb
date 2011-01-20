// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_text_button.h"

#include "app/l10n_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/views/infobars/infobar_button_border.h"
#include "ui/base/resource/resource_bundle.h"

// static
InfoBarTextButton* InfoBarTextButton::Create(views::ButtonListener* listener,
                                             const string16& text) {
  return new InfoBarTextButton(listener, text);
}

// static
InfoBarTextButton* InfoBarTextButton::CreateWithMessageID(
    views::ButtonListener* listener, int message_id) {
  return new InfoBarTextButton(listener,
                               l10n_util::GetStringUTF16(message_id));
}

// static
InfoBarTextButton* InfoBarTextButton::CreateWithMessageIDAndParam(
      views::ButtonListener* listener, int message_id, const string16& param) {
  return new InfoBarTextButton(listener,
                               l10n_util::GetStringFUTF16(message_id, param));
}

InfoBarTextButton::~InfoBarTextButton() {
}

bool InfoBarTextButton::OnMousePressed(const views::MouseEvent& e) {
  return views::CustomButton::OnMousePressed(e);
}

InfoBarTextButton::InfoBarTextButton(views::ButtonListener* listener,
                                     const string16& text)
    // Don't use text to construct TextButton because we need to set font
    // before setting text so that the button will resize to fit entire text.
    : TextButton(listener, std::wstring()) {
  set_border(new InfoBarButtonBorder);
  SetNormalHasBorder(true);  // Normal button state has border.
  SetAnimationDuration(0);  // Disable animation during state change.
  // Set font colors for different states.
  SetEnabledColor(SK_ColorBLACK);
  SetHighlightColor(SK_ColorBLACK);
  SetHoverColor(SK_ColorBLACK);
  // Set font then text, then size button to fit text.
  SetFont(
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::MediumFont));
  SetText(UTF16ToWideHack(text));
  ClearMaxTextSize();
  SizeToPreferredSize();
}
