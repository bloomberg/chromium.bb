// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_TRIGGERS_SUSPICIOUS_SITE_TRIGGER_H_
#define COMPONENTS_SAFE_BROWSING_TRIGGERS_SUSPICIOUS_SITE_TRIGGER_H_

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class PrefService;

namespace history {
class HistoryService;
}

namespace net {
class URLRequestContextGetter;
}

namespace safe_browsing {
class TriggerManager;

// This class watches tab-level events such as the start and end of a page
// load, and also listens for events from the SuspiciousSiteURLThrottle that
// indicate there was a hit on the suspicious site list. This trigger is
// repsonsible for creating reports about the page at the right time, based on
// the sequence of such events.
class SuspiciousSiteTrigger
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SuspiciousSiteTrigger> {
 public:
  ~SuspiciousSiteTrigger() override;

  static void CreateForWebContents(
      content::WebContents* web_contents,
      TriggerManager* trigger_manager,
      PrefService* prefs,
      net::URLRequestContextGetter* request_context,
      history::HistoryService* history_service);

 private:
  friend class content::WebContentsUserData<SuspiciousSiteTrigger>;

  SuspiciousSiteTrigger(content::WebContents* web_contents,
                        TriggerManager* trigger_manager,
                        PrefService* prefs,
                        net::URLRequestContextGetter* request_context,
                        history::HistoryService* history_service);

  // TriggerManager gets called if this trigger detects a suspicious site and
  // wants to collect data abou tit. Not owned.
  TriggerManager* trigger_manager_;

  PrefService* prefs_;
  net::URLRequestContextGetter* request_context_;
  history::HistoryService* history_service_;

  DISALLOW_COPY_AND_ASSIGN(SuspiciousSiteTrigger);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_TRIGGERS_SUSPICIOUS_SITE_TRIGGER_H_
