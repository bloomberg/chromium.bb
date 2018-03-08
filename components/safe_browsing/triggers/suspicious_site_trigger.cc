// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/triggers/suspicious_site_trigger.h"

#include "components/history/core/browser/history_service.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/triggers/trigger_manager.h"
#include "content/public/browser/web_contents.h"
#include "net/url_request/url_request_context_getter.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(safe_browsing::SuspiciousSiteTrigger);

namespace safe_browsing {
SuspiciousSiteTrigger::SuspiciousSiteTrigger(
    content::WebContents* web_contents,
    TriggerManager* trigger_manager,
    PrefService* prefs,
    net::URLRequestContextGetter* request_context,
    history::HistoryService* history_service)
    : content::WebContentsObserver(web_contents),
      trigger_manager_(trigger_manager),
      prefs_(prefs),
      request_context_(request_context),
      history_service_(history_service) {}

SuspiciousSiteTrigger::~SuspiciousSiteTrigger() {}

// static
void SuspiciousSiteTrigger::CreateForWebContents(
    content::WebContents* web_contents,
    TriggerManager* trigger_manager,
    PrefService* prefs,
    net::URLRequestContextGetter* request_context,
    history::HistoryService* history_service) {
  if (!FromWebContents(web_contents)) {
    web_contents->SetUserData(UserDataKey(),
                              base::WrapUnique(new SuspiciousSiteTrigger(
                                  web_contents, trigger_manager, prefs,
                                  request_context, history_service)));
  }
}

}  // namespace safe_browsing
