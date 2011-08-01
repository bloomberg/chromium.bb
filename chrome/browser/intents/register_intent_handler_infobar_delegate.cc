// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/intents/register_intent_handler_infobar_delegate.h"

#include "base/logging.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

RegisterIntentHandlerInfoBarDelegate::RegisterIntentHandlerInfoBarDelegate(
    TabContents* tab_contents)
    : ConfirmInfoBarDelegate(tab_contents),
      tab_contents_(tab_contents) {
}

InfoBarDelegate::Type
    RegisterIntentHandlerInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

string16 RegisterIntentHandlerInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_REGISTER_PROTOCOL_HANDLER_CONFIRM,
                                    string16(), string16());
}

string16 RegisterIntentHandlerInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  if (button == BUTTON_OK) {
    return l10n_util::GetStringFUTF16(IDS_REGISTER_INTENT_HANDLER_ACCEPT,
                                      string16());
  }

  DCHECK(button == BUTTON_CANCEL);
  return l10n_util::GetStringUTF16(IDS_REGISTER_INTENT_HANDLER_DENY);
}

string16 RegisterIntentHandlerInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

bool RegisterIntentHandlerInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  // TODO(jhawkins): Open the Web Intents Help Center article once it is
  // written.
  // TODO(jhawkins): Add associated bug for the article here.
  return false;
}
