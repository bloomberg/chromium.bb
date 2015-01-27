// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_RESOURCE_IOS_WEB_RESOURCE_SERVICE_H_
#define IOS_CHROME_BROWSER_WEB_RESOURCE_IOS_WEB_RESOURCE_SERVICE_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/web_resource/web_resource_service.h"

// iOS implementation of WebResourceService.
// IOSWebResourceService does not parses JSON out of process, because of
// platform limitations.
class IOSWebResourceService : public web_resource::WebResourceService {
 public:
  IOSWebResourceService(PrefService* prefs,
                        const GURL& web_resource_server,
                        bool apply_locale_to_url,
                        const char* last_update_time_pref_name,
                        int start_fetch_delay_ms,
                        int cache_update_delay_ms);

 protected:
  ~IOSWebResourceService() override;

 private:
  // Parses JSON in a background thread.
  static base::Closure ParseJSONOnBackgroundThread(
      const std::string& data,
      const SuccessCallback& success_callback,
      const ErrorCallback& error_callback);

  // WebResourceService implementation:
  void ParseJSON(const std::string& data,
                 const SuccessCallback& success_callback,
                 const ErrorCallback& error_callback) override;

  DISALLOW_COPY_AND_ASSIGN(IOSWebResourceService);
};

#endif  // IOS_CHROME_BROWSER_WEB_RESOURCE_IOS_WEB_RESOURCE_SERVICE_H_
