// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/triggers/trigger_manager.h"

#include "components/safe_browsing/base_ui_manager.h"
#include "components/safe_browsing/browser/threat_details.h"
#include "content/public/browser/web_contents.h"
#include "net/url_request/url_request_context_getter.h"

namespace safe_browsing {

TriggerManager::TriggerManager(BaseUIManager* ui_manager)
    : ui_manager_(ui_manager) {}

TriggerManager::~TriggerManager() {}

ThreatDetails* TriggerManager::StartCollectingThreatDetails(
    content::WebContents* web_contents,
    const security_interstitials::UnsafeResource& resource,
    net::URLRequestContextGetter* request_context_getter,
    history::HistoryService* history_service) {
  return ThreatDetails::NewThreatDetails(ui_manager_, web_contents, resource,
                                         request_context_getter,
                                         history_service);
}

}  // namespace safe_browsing
