// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/intents/register_intent_handler_infobar_delegate.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/intents/web_intents_registry.h"
#include "chrome/browser/intents/web_intents_registry_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

RegisterIntentHandlerInfoBarDelegate::RegisterIntentHandlerInfoBarDelegate(
    TabContents* tab_contents, const WebIntentServiceData& service)
    : ConfirmInfoBarDelegate(tab_contents),
      tab_contents_(tab_contents),
      profile_(Profile::FromBrowserContext(tab_contents->browser_context())),
      service_(service) {
}

InfoBarDelegate::Type
    RegisterIntentHandlerInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

string16 RegisterIntentHandlerInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(
      IDS_REGISTER_INTENT_HANDLER_CONFIRM,
      service_.title,
      UTF8ToUTF16(service_.service_url.host()));
}

string16 RegisterIntentHandlerInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  if (button == BUTTON_OK) {
    return l10n_util::GetStringFUTF16(IDS_REGISTER_INTENT_HANDLER_ACCEPT,
                                      UTF8ToUTF16(service_.service_url.host()));
  }

  DCHECK(button == BUTTON_CANCEL);
  return l10n_util::GetStringUTF16(IDS_REGISTER_INTENT_HANDLER_DENY);
}

bool RegisterIntentHandlerInfoBarDelegate::Accept() {
  WebIntentsRegistry* registry =
      WebIntentsRegistryFactory::GetForProfile(profile_);
  registry->RegisterIntentProvider(service_);
  return true;
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
