// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/custom_handlers/register_protocol_handler_infobar_delegate.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::OpenURLParams;
using content::Referrer;
using content::UserMetricsAction;

// static
void RegisterProtocolHandlerInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    ProtocolHandlerRegistry* registry,
    const ProtocolHandler& handler) {
  content::RecordAction(
      content::UserMetricsAction("RegisterProtocolHandler.InfoBar_Shown"));

  scoped_ptr<InfoBarDelegate> infobar(
      new RegisterProtocolHandlerInfoBarDelegate(infobar_service, registry,
                                                 handler));

  for (size_t i = 0; i < infobar_service->GetInfoBarCount(); ++i) {
    RegisterProtocolHandlerInfoBarDelegate* existing_delegate =
        infobar_service->GetInfoBarDelegateAt(i)->
            AsRegisterProtocolHandlerInfoBarDelegate();
    if ((existing_delegate != NULL) &&
        existing_delegate->handler_.IsEquivalent(handler)) {
      infobar_service->ReplaceInfoBar(existing_delegate, infobar.Pass());
      return;
    }
  }

  infobar_service->AddInfoBar(infobar.Pass());
}

InfoBarDelegate::InfoBarAutomationType
    RegisterProtocolHandlerInfoBarDelegate::GetInfoBarAutomationType() const {
  return RPH_INFOBAR;
}

InfoBarDelegate::Type
RegisterProtocolHandlerInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

string16 RegisterProtocolHandlerInfoBarDelegate::GetMessageText() const {
  ProtocolHandler old_handler = registry_->GetHandlerFor(handler_.protocol());
  return !old_handler.IsEmpty() ?
      l10n_util::GetStringFUTF16(IDS_REGISTER_PROTOCOL_HANDLER_CONFIRM_REPLACE,
          handler_.title(), UTF8ToUTF16(handler_.url().host()),
          GetProtocolName(handler_), old_handler.title()) :
      l10n_util::GetStringFUTF16(IDS_REGISTER_PROTOCOL_HANDLER_CONFIRM,
          handler_.title(), UTF8ToUTF16(handler_.url().host()),
          GetProtocolName(handler_));
}

RegisterProtocolHandlerInfoBarDelegate::RegisterProtocolHandlerInfoBarDelegate(
    InfoBarService* infobar_service,
    ProtocolHandlerRegistry* registry,
    const ProtocolHandler& handler)
    : ConfirmInfoBarDelegate(infobar_service),
      registry_(registry),
      handler_(handler) {
}

string16 RegisterProtocolHandlerInfoBarDelegate::GetProtocolName(
    const ProtocolHandler& handler) const {
  if (handler.protocol() == "mailto")
    return l10n_util::GetStringUTF16(IDS_REGISTER_PROTOCOL_HANDLER_MAILTO_NAME);
  if (handler.protocol() == "webcal")
    return l10n_util::GetStringUTF16(IDS_REGISTER_PROTOCOL_HANDLER_WEBCAL_NAME);
  return UTF8ToUTF16(handler.protocol());
}

string16 RegisterProtocolHandlerInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return (button == BUTTON_OK) ?
      l10n_util::GetStringFUTF16(IDS_REGISTER_PROTOCOL_HANDLER_ACCEPT,
                                 handler_.title()) :
      l10n_util::GetStringUTF16(IDS_REGISTER_PROTOCOL_HANDLER_DENY);
}

bool RegisterProtocolHandlerInfoBarDelegate::NeedElevation(
    InfoBarButton button) const {
  return button == BUTTON_OK;
}

bool RegisterProtocolHandlerInfoBarDelegate::Accept() {
  content::RecordAction(
      UserMetricsAction("RegisterProtocolHandler.Infobar_Accept"));
  registry_->OnAcceptRegisterProtocolHandler(handler_);
  return true;
}

bool RegisterProtocolHandlerInfoBarDelegate::Cancel() {
  content::RecordAction(
      UserMetricsAction("RegisterProtocolHandler.InfoBar_Deny"));
  registry_->OnIgnoreRegisterProtocolHandler(handler_);
  return true;
}

string16 RegisterProtocolHandlerInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

bool RegisterProtocolHandlerInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  content::RecordAction(
      UserMetricsAction("RegisterProtocolHandler.InfoBar_LearnMore"));
  OpenURLParams params(
      GURL(chrome::kLearnMoreRegisterProtocolHandlerURL),
      Referrer(),
      (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition,
      content::PAGE_TRANSITION_LINK,
      false);
  owner()->GetWebContents()->OpenURL(params);
  return false;
}

RegisterProtocolHandlerInfoBarDelegate*
    RegisterProtocolHandlerInfoBarDelegate::
        AsRegisterProtocolHandlerInfoBarDelegate() {
  return this;
}
