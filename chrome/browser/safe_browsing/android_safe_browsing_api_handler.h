// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Glue to pass Safe Browsing API requests between
// RemoteSafeBrowsingDatabaseManager and Java-based API to check URLs.

#ifndef CHROME_BROWSER_SAFE_BROWSING_ANDROID_SAFE_BROWSING_API_HANDLER_H_
#define CHROME_BROWSER_SAFE_BROWSING_ANDROID_SAFE_BROWSING_API_HANDLER_H_

#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "url/gurl.h"

typedef base::Callback<void(SBThreatType sb_threat_type,
                            const std::string& metadata)> URLCheckCallback;

class AndroidSafeBrowsingAPIHandler {
 public:
  AndroidSafeBrowsingAPIHandler();

  // Makes Native->Java call and invokes callback when check is done.
  // Returns false if it fails to start.
  bool StartURLCheck(
      const URLCheckCallback& callback,
      const GURL& url,
      const std::vector<SBThreatType>& threat_types);

  // Java->Native call, invoked when a check is done.  |callback_id| is an
  // int form of pointer to a URLCheckCallback that will be called and then
  // deleted here.
  static void OnURLCheckDone(jlong callback_id,
                             jboolean isSuccessful,
                             jstring metadata);

 protected:
  jobject j_context_;
  std::string api_key_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AndroidSafeBrowsingAPIHandler);
};
#endif  // CHROME_BROWSER_SAFE_BROWSING_ANDROID_SAFE_BROWSING_API_HANDLER_H_
