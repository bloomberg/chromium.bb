// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_COMMON_WEBSERVICE_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_COMMON_WEBSERVICE_SEARCH_PROVIDER_H_

#include "base/callback_forward.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"

namespace app_list {

// Helper class for webservice based searches.
class WebserviceSearchProvider : public SearchProvider {
 public:
  WebserviceSearchProvider();
  virtual ~WebserviceSearchProvider();

  void StartThrottledQuery(const base::Closure& start_query);
  bool IsSensitiveInput(const string16& query);

  void set_use_throttling(bool use) { use_throttling_ = use; }

 private:
  // The timestamp when the last key event happened.
  base::Time last_keytyped_;

  // The timer to throttle QPS to the webstore search .
  base::OneShotTimer<WebserviceSearchProvider> query_throttler_;

  // The flag for tests. It prevents the throttling If set to false.
  bool use_throttling_;

  DISALLOW_COPY_AND_ASSIGN(WebserviceSearchProvider);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_COMMON_WEBSERVICE_SEARCH_PROVIDER_H_
