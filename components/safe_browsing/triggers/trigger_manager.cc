// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/triggers/trigger_manager.h"

#include "components/safe_browsing/base_ui_manager.h"
#include "components/safe_browsing/browser/threat_details.h"
#include "components/security_interstitials/content/unsafe_resource.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request_context_getter.h"

namespace safe_browsing {

TriggerManager::TriggerManager(BaseUIManager* ui_manager)
    : ui_manager_(ui_manager) {}

TriggerManager::~TriggerManager() {}

bool TriggerManager::StartCollectingThreatDetails(
    content::WebContents* web_contents,
    const security_interstitials::UnsafeResource& resource,
    net::URLRequestContextGetter* request_context_getter,
    history::HistoryService* history_service) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Ensure we're not already collecting data on this tab.
  if (base::ContainsKey(data_collectors_map_, web_contents))
    return false;

  data_collectors_map_[web_contents] = scoped_refptr<ThreatDetails>(
      ThreatDetails::NewThreatDetails(ui_manager_, web_contents, resource,
                                      request_context_getter, history_service));
  return true;
}

bool TriggerManager::FinishCollectingThreatDetails(
    content::WebContents* web_contents,
    const base::TimeDelta& delay,
    bool did_proceed,
    int num_visits) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Make sure there's a data collector running on this tab.
  if (!base::ContainsKey(data_collectors_map_, web_contents))
    return false;

  // Find the data collector and tell it to finish collecting data, and then
  // remove it from our map. We release ownership of the data collector here but
  // it will live until the end of the FinishCollection call because it
  // implements RefCountedThreadSafe.
  scoped_refptr<ThreatDetails> threat_details =
      data_collectors_map_[web_contents];
  content::BrowserThread::PostDelayedTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&ThreatDetails::FinishCollection, threat_details,
                     did_proceed, num_visits),
      delay);
  data_collectors_map_.erase(web_contents);

  return true;
}

}  // namespace safe_browsing
