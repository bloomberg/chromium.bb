// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/intents/register_intent_handler_infobar_delegate.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/intents/web_intents_registry.h"
#include "chrome/browser/intents/web_intents_registry_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

RegisterIntentHandlerInfoBarDelegate::RegisterIntentHandlerInfoBarDelegate(
    InfoBarTabHelper* infobar_helper,
    WebIntentsRegistry* registry,
    const webkit_glue::WebIntentServiceData& service,
    FaviconService* favicon_service,
    const GURL& origin_url)
    : ConfirmInfoBarDelegate(infobar_helper),
      registry_(registry),
      service_(service),
      favicon_service_(favicon_service),
      origin_url_(origin_url) {
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
  registry_->RegisterIntentProvider(service_);

  // Register a temporary FavIcon in case we never visited the provider page.
  if (favicon_service_ && origin_url_ != service_.service_url)
    favicon_service_->CloneFavicon(origin_url_, service_.service_url);

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

namespace {

// Helper continuation for MaybeShowIntentInfoBar.
void CheckProvider(InfoBarTabHelper* infobar_helper,
                   WebIntentsRegistry* registry,
                   const webkit_glue::WebIntentServiceData& service,
                   FaviconService* favicon_service,
                   const GURL& origin_url,
                   bool provider_exists) {
  if (!provider_exists) {
    infobar_helper->AddInfoBar(new RegisterIntentHandlerInfoBarDelegate(
        infobar_helper, registry, service, favicon_service, origin_url));
  }
}

}  // namespace

// static
void RegisterIntentHandlerInfoBarDelegate::MaybeShowIntentInfoBar(
    InfoBarTabHelper* infobar_helper,
    WebIntentsRegistry* registry,
    const webkit_glue::WebIntentServiceData& service,
    FaviconService* favicon_service,
    const GURL& origin_url) {
  DCHECK(infobar_helper);
  DCHECK(registry);
  registry->IntentProviderExists(service,
                                 base::Bind(&CheckProvider,
                                            base::Unretained(infobar_helper),
                                            registry,
                                            service,
                                            favicon_service,
                                            origin_url));
}
