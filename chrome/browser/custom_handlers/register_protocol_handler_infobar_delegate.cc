// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/custom_handlers/register_protocol_handler_infobar_delegate.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

// static
void RegisterProtocolHandlerInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    ProtocolHandlerRegistry* registry,
    const ProtocolHandler& handler) {
  content::RecordAction(
      base::UserMetricsAction("RegisterProtocolHandler.InfoBar_Shown"));

  scoped_ptr<infobars::InfoBar> infobar(
      ConfirmInfoBarDelegate::CreateInfoBar(scoped_ptr<ConfirmInfoBarDelegate>(
          new RegisterProtocolHandlerInfoBarDelegate(registry, handler))));

  for (size_t i = 0; i < infobar_service->infobar_count(); ++i) {
    infobars::InfoBar* existing_infobar = infobar_service->infobar_at(i);
    RegisterProtocolHandlerInfoBarDelegate* existing_delegate =
        existing_infobar->delegate()->
            AsRegisterProtocolHandlerInfoBarDelegate();
    if ((existing_delegate != NULL) &&
        existing_delegate->handler_.IsEquivalent(handler)) {
      infobar_service->ReplaceInfoBar(existing_infobar, infobar.Pass());
      return;
    }
  }

  infobar_service->AddInfoBar(infobar.Pass());
}

RegisterProtocolHandlerInfoBarDelegate::RegisterProtocolHandlerInfoBarDelegate(
    ProtocolHandlerRegistry* registry,
    const ProtocolHandler& handler)
    : ConfirmInfoBarDelegate(),
      registry_(registry),
      handler_(handler) {
}

RegisterProtocolHandlerInfoBarDelegate::
    ~RegisterProtocolHandlerInfoBarDelegate() {
}

infobars::InfoBarDelegate::InfoBarAutomationType
RegisterProtocolHandlerInfoBarDelegate::GetInfoBarAutomationType() const {
  return RPH_INFOBAR;
}

infobars::InfoBarDelegate::Type
RegisterProtocolHandlerInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

RegisterProtocolHandlerInfoBarDelegate*
    RegisterProtocolHandlerInfoBarDelegate::
        AsRegisterProtocolHandlerInfoBarDelegate() {
  return this;
}

base::string16 RegisterProtocolHandlerInfoBarDelegate::GetMessageText() const {
  ProtocolHandler old_handler = registry_->GetHandlerFor(handler_.protocol());
  return old_handler.IsEmpty() ?
      l10n_util::GetStringFUTF16(
          IDS_REGISTER_PROTOCOL_HANDLER_CONFIRM,
          base::UTF8ToUTF16(handler_.url().host()),
          GetProtocolName(handler_)) :
      l10n_util::GetStringFUTF16(
          IDS_REGISTER_PROTOCOL_HANDLER_CONFIRM_REPLACE,
          base::UTF8ToUTF16(handler_.url().host()),
          GetProtocolName(handler_),
          base::UTF8ToUTF16(old_handler.url().host()));
}

base::string16 RegisterProtocolHandlerInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return (button == BUTTON_OK) ?
      l10n_util::GetStringUTF16(IDS_REGISTER_PROTOCOL_HANDLER_ACCEPT) :
      l10n_util::GetStringUTF16(IDS_REGISTER_PROTOCOL_HANDLER_DENY);
}

bool RegisterProtocolHandlerInfoBarDelegate::OKButtonTriggersUACPrompt() const {
  return true;
}

bool RegisterProtocolHandlerInfoBarDelegate::Accept() {
  content::RecordAction(
      base::UserMetricsAction("RegisterProtocolHandler.Infobar_Accept"));
  registry_->OnAcceptRegisterProtocolHandler(handler_);
  return true;
}

bool RegisterProtocolHandlerInfoBarDelegate::Cancel() {
  content::RecordAction(
      base::UserMetricsAction("RegisterProtocolHandler.InfoBar_Deny"));
  registry_->OnIgnoreRegisterProtocolHandler(handler_);
  return true;
}

base::string16 RegisterProtocolHandlerInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

bool RegisterProtocolHandlerInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  content::RecordAction(
      base::UserMetricsAction("RegisterProtocolHandler.InfoBar_LearnMore"));
  InfoBarService::WebContentsFromInfoBar(infobar())->OpenURL(
      content::OpenURLParams(
          GURL(chrome::kLearnMoreRegisterProtocolHandlerURL),
          content::Referrer(),
          (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition,
          content::PAGE_TRANSITION_LINK, false));
  return false;
}

base::string16 RegisterProtocolHandlerInfoBarDelegate::GetProtocolName(
    const ProtocolHandler& handler) const {
  if (handler.protocol() == "mailto")
    return l10n_util::GetStringUTF16(IDS_REGISTER_PROTOCOL_HANDLER_MAILTO_NAME);
  if (handler.protocol() == "webcal")
    return l10n_util::GetStringUTF16(IDS_REGISTER_PROTOCOL_HANDLER_WEBCAL_NAME);
  return base::UTF8ToUTF16(handler.protocol());
}
