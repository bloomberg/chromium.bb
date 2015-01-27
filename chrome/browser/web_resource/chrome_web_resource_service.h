// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_RESOURCE_CHROME_WEB_RESOURCE_SERVICE_H_
#define CHROME_BROWSER_WEB_RESOURCE_CHROME_WEB_RESOURCE_SERVICE_H_

#include "base/macros.h"
#include "components/web_resource/web_resource_service.h"

// Chrome implementation of WebResourceService.
// ChromeWebResourceService safely parses JSON out of process.
class ChromeWebResourceService : public web_resource::WebResourceService {
 public:
  ChromeWebResourceService(PrefService* prefs,
                           const GURL& web_resource_server,
                           bool apply_locale_to_url,
                           const char* last_update_time_pref_name,
                           int start_fetch_delay_ms,
                           int cache_update_delay_ms);

 protected:
  ~ChromeWebResourceService() override;

 private:
  // WebResourceService implementation:
  void ParseJSON(const std::string& data,
                 const SuccessCallback& success_callback,
                 const ErrorCallback& error_callback) override;

  DISALLOW_COPY_AND_ASSIGN(ChromeWebResourceService);
};

#endif  // CHROME_BROWSER_WEB_RESOURCE_CHROME_WEB_RESOURCE_SERVICE_H_
