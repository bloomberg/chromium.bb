// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/intents/register_intent_handler_infobar_delegate.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/intents/web_intents_registry.h"
#include "chrome/browser/intents/web_intents_registry_factory.h"
#include "chrome/browser/intents/web_intents_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"


// static
void RegisterIntentHandlerInfoBarDelegate::Create(
    content::WebContents* web_contents,
    const webkit_glue::WebIntentServiceData& data) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (profile->IsOffTheRecord())
    return;

  if (!web_intents::IsWebIntentsEnabledForProfile(profile))
    return;

  FaviconService* favicon_service = FaviconServiceFactory::GetForProfile(
      profile, Profile::EXPLICIT_ACCESS);

  WebIntentsRegistry* registry =
      WebIntentsRegistryFactory::GetForProfile(profile);
  registry->IntentServiceExists(
      data, base::Bind(
          &CreateContinuation,
          base::Unretained(InfoBarService::FromWebContents(web_contents)),
          registry,
          data,
          favicon_service,
          web_contents->GetURL()));
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
  registry_->RegisterIntentService(service_);

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

RegisterIntentHandlerInfoBarDelegate::RegisterIntentHandlerInfoBarDelegate(
    InfoBarService* infobar_service,
    WebIntentsRegistry* registry,
    const webkit_glue::WebIntentServiceData& service,
    FaviconService* favicon_service,
    const GURL& origin_url)
    : ConfirmInfoBarDelegate(infobar_service),
      registry_(registry),
      service_(service),
      favicon_service_(favicon_service),
      origin_url_(origin_url) {
}

// static
void RegisterIntentHandlerInfoBarDelegate::CreateContinuation(
    InfoBarService* infobar_service,
    WebIntentsRegistry* registry,
    const webkit_glue::WebIntentServiceData& service,
    FaviconService* favicon_service,
    const GURL& origin_url,
    bool provider_exists) {
  if (!provider_exists) {
    infobar_service->AddInfoBar(scoped_ptr<InfoBarDelegate>(
        new RegisterIntentHandlerInfoBarDelegate(
            infobar_service, registry, service, favicon_service, origin_url)));
  }
}
