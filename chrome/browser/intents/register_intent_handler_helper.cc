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
void Browser::RegisterIntentHandlerHelper(
    WebContents* tab,
    const webkit_glue::WebIntentServiceData& data,
    bool user_gesture) {
  TabContentsWrapper* tcw =
      TabContentsWrapper::GetCurrentWrapperForContents(tab);
  if (!tcw || tcw->profile()->IsOffTheRecord())
    return;

  if (!web_intents::IsWebIntentsEnabledForProfile(tcw->profile()))
    return;

  FaviconService* favicon_service =
      tcw->profile()->GetFaviconService(Profile::EXPLICIT_ACCESS);

  RegisterIntentHandlerInfoBarDelegate::MaybeShowIntentInfoBar(
      tcw->infobar_tab_helper(),
      WebIntentsRegistryFactory::GetForProfile(tcw->profile()),
      data,
      favicon_service,
      tab->GetURL());
}
