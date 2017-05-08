// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_TRIGGERS_TRIGGER_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_TRIGGERS_TRIGGER_MANAGER_H_

#include "base/macros.h"
#include "components/security_interstitials/content/unsafe_resource.h"

namespace history {
class HistoryService;
}

namespace net {
class URLRequestContextGetter;
}

namespace safe_browsing {

class BaseUIManager;
class ThreatDetails;

// This class manages SafeBrowsing data-reporting triggers. Triggers are
// activated for users opted-in to Extended Reporting and when security-related
// data collection is required.
//
// The TriggerManager has two main responsibilities: 1) ensuring triggers only
// run when appropriate, by honouring user opt-ins and incognito state, and 2)
// tracking how often triggers fire and throttling them when necessary.
class TriggerManager {
 public:
  TriggerManager(BaseUIManager* ui_manager);
  ~TriggerManager();

  ThreatDetails* StartCollectingThreatDetails(
      content::WebContents* web_contents,
      const security_interstitials::UnsafeResource& resource,
      net::URLRequestContextGetter* request_context_getter,
      history::HistoryService* history_service);

 private:
  // The UI manager is used to send reports to Google. Not owned.
  // TODO(lpz): we may only need a the PingManager here.
  BaseUIManager* ui_manager_;

  DISALLOW_COPY_AND_ASSIGN(TriggerManager);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_TRIGGERS_TRIGGER_MANAGER_H_
