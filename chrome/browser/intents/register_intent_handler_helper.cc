// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/intents/register_intent_handler_infobar_delegate.h"
#include "chrome/browser/intents/web_intents_registry_factory.h"
#include "chrome/browser/intents/web_intents_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/public/browser/web_contents.h"
#include "webkit/glue/web_intent_service_data.h"

using content::WebContents;

// static
void Browser::RegisterIntentHandlerHelper(WebContents* tab,
                                          const string16& action,
                                          const string16& type,
                                          const string16& href,
                                          const string16& title,
                                          const string16& disposition) {
  TabContentsWrapper* tcw = TabContentsWrapper::GetCurrentWrapperForContents(
      tab);
  if (!tcw || tcw->profile()->IsOffTheRecord())
    return;

  if (!web_intents::IsWebIntentsEnabledForProfile(tcw->profile()))
    return;

  FaviconService* favicon_service =
      tcw->profile()->GetFaviconService(Profile::EXPLICIT_ACCESS);

  // |href| can be relative to originating URL. Resolve if necessary.
  GURL service_url(href);
  if (!service_url.is_valid()) {
    const GURL& url = tab->GetURL();
    service_url = url.Resolve(href);
  }

  webkit_glue::WebIntentServiceData service;
  service.service_url = service_url;
  service.action = action;
  service.type = type;
  service.title = title;
  service.setDisposition(disposition);

  RegisterIntentHandlerInfoBarDelegate::MaybeShowIntentInfoBar(
      tcw->infobar_tab_helper(),
      WebIntentsRegistryFactory::GetForProfile(tcw->profile()),
      service,
      favicon_service,
      tab->GetURL());
}
