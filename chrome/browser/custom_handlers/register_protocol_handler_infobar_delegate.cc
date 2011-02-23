// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/custom_handlers/register_protocol_handler_infobar_delegate.h"

#include "base/metrics/histogram.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_cc_infobar.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/common/pref_names.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

RegisterProtocolHandlerInfoBarDelegate::RegisterProtocolHandlerInfoBarDelegate(
    TabContents* tab_contents, ProtocolHandlerRegistry* registry,
    ProtocolHandler* handler)
    : ConfirmInfoBarDelegate(tab_contents),
      tab_contents_(tab_contents),
      registry_(registry),
      handler_(handler) {
}

void RegisterProtocolHandlerInfoBarDelegate::AttemptRegisterProtocolHandler(
    TabContents* tab_contents,
    ProtocolHandlerRegistry* registry,
    ProtocolHandler* handler) {
  if (!registry->CanSchemeBeOverridden(handler->protocol())) {
    return;
  }

  if (registry->IsAlreadyRegistered(handler)) {
      tab_contents->AddInfoBar(new SimpleAlertInfoBarDelegate(tab_contents,
          NULL,
          l10n_util::GetStringFUTF16(
              IDS_REGISTER_PROTOCOL_HANDLER_ALREADY_REGISTERED,
              handler->title(), UTF8ToUTF16(handler->protocol())), true));
    return;
  }

  RegisterProtocolHandlerInfoBarDelegate* delegate =
      new RegisterProtocolHandlerInfoBarDelegate(tab_contents,
                                                 registry,
                                                 handler);
  tab_contents->AddInfoBar(delegate);
}

bool RegisterProtocolHandlerInfoBarDelegate::ShouldExpire(
    const NavigationController::LoadCommittedDetails& details) const {
  // The user has submitted a form, causing the page to navigate elsewhere. We
  // don't want the infobar to be expired at this point, because the user won't
  // get a chance to answer the question.
  return false;
}

void RegisterProtocolHandlerInfoBarDelegate::InfoBarClosed() {
  delete this;
}

string16 RegisterProtocolHandlerInfoBarDelegate::GetMessageText() const {
  ProtocolHandler* old_handler = registry_->GetHandlerFor(handler_->protocol());
  if (old_handler) {
    return l10n_util::GetStringFUTF16(
        IDS_REGISTER_PROTOCOL_HANDLER_CONFIRM_REPLACE, handler_->title(),
        UTF8ToUTF16(handler_->url().host()), UTF8ToUTF16(handler_->protocol()),
        old_handler->title());
  }
  return l10n_util::GetStringFUTF16(IDS_REGISTER_PROTOCOL_HANDLER_CONFIRM,
      handler_->title(), UTF8ToUTF16(handler_->url().host()),
      UTF8ToUTF16(handler_->protocol()));
}

SkBitmap* RegisterProtocolHandlerInfoBarDelegate::GetIcon() const {
  return NULL;
}

int RegisterProtocolHandlerInfoBarDelegate::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

string16 RegisterProtocolHandlerInfoBarDelegate::GetButtonLabel(
    ConfirmInfoBarDelegate::InfoBarButton button) const {
  if (button == BUTTON_OK) {
    return l10n_util::GetStringFUTF16(IDS_REGISTER_PROTOCOL_HANDLER_ACCEPT,
                                      handler_->title());
  } else if (button == BUTTON_CANCEL) {
    return l10n_util::GetStringUTF16(IDS_REGISTER_PROTOCOL_HANDLER_DENY);
  } else {
    NOTREACHED();
  }

  return string16();
}

bool RegisterProtocolHandlerInfoBarDelegate::Accept() {
  registry_->OnAcceptRegisterProtocolHandler(handler_);
  return true;
}

bool RegisterProtocolHandlerInfoBarDelegate::Cancel() {
  registry_->OnDenyRegisterProtocolHandler(handler_);
  return true;
}

string16 RegisterProtocolHandlerInfoBarDelegate::GetLinkText() {
  // TODO(koz): Make this a 'learn more' link.
  return string16();
}

bool RegisterProtocolHandlerInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  return false;
}

InfoBarDelegate::Type RegisterProtocolHandlerInfoBarDelegate::GetInfoBarType()
    const {
  return PAGE_ACTION_TYPE;
}
