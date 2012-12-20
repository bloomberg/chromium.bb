// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/intents/register_intent_handler_infobar_delegate.h"
#include "chrome/browser/intents/web_intents_registry_factory.h"
#include "chrome/browser/intents/web_intents_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/web_contents.h"
#include "webkit/glue/web_intent_service_data.h"

using content::WebContents;

// static
void Browser::RegisterIntentHandlerHelper(
    WebContents* web_contents,
    const webkit_glue::WebIntentServiceData& data,
    bool user_gesture) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (profile->IsOffTheRecord())
    return;

  if (!web_intents::IsWebIntentsEnabledForProfile(profile))
    return;

  FaviconService* favicon_service = FaviconServiceFactory::GetForProfile(
      profile, Profile::EXPLICIT_ACCESS);

  RegisterIntentHandlerInfoBarDelegate::MaybeShowIntentInfoBar(
      InfoBarService::FromWebContents(web_contents),
      WebIntentsRegistryFactory::GetForProfile(profile),
      data,
      favicon_service,
      web_contents->GetURL());
}
