// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Glue to pass Safe Browsing API requests between
// RemoteSafeBrowsingDatabaseManager and Java-based API to check URLs.

#ifndef COMPONENTS_SAFE_BROWSING_DB_SAFE_BROWSING_API_HANDLER_H_
#define COMPONENTS_SAFE_BROWSING_DB_SAFE_BROWSING_API_HANDLER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "components/safe_browsing_db/util.h"
#include "url/gurl.h"

namespace safe_browsing {

class SafeBrowsingApiHandler {
 public:
  // Singleton interface.
  static void SetInstance(SafeBrowsingApiHandler* instance);
  static SafeBrowsingApiHandler* GetInstance();

  typedef base::Callback<void(SBThreatType sb_threat_type,
                              const ThreatMetadata& metadata)>
      URLCheckCallbackMeta;

  // TODO(nparker): Remove this as part of crbug/589610.
  typedef base::Callback<void(SBThreatType sb_threat_type,
                              const std::string& metadata_str)>
      URLCheckCallback;

  // Makes Native->Java call and invokes callback when check is done.
  // TODO(nparker): Switch this back to pure virtual. crbug/589610.
  virtual void StartURLCheck(const URLCheckCallbackMeta& callback,
                             const GURL& url,
                             const std::vector<SBThreatType>& threat_types);

  // TODO(nparker): Remove this as part of crbug/589610.
  virtual void StartURLCheck(const URLCheckCallback& callback,
                             const GURL& url,
                             const std::vector<SBThreatType>& threat_types) {};

  virtual ~SafeBrowsingApiHandler() {}

 private:
  // Pointer not owned.
  static SafeBrowsingApiHandler* instance_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_DB_SAFE_BROWSING_API_HANDLER_H_
