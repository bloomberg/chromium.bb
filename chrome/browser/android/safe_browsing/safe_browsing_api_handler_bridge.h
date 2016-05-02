// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Glue to pass Safe Browsing API requests between Chrome and GMSCore

#ifndef CHROME_BROWSER_ANDROID_SAFE_BROWSING_SAFE_BROWSING_API_HANDLER_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_SAFE_BROWSING_SAFE_BROWSING_API_HANDLER_BRIDGE_H_

#include <jni.h>

#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "components/safe_browsing_db/safe_browsing_api_handler.h"
#include "url/gurl.h"

namespace safe_browsing {
bool RegisterSafeBrowsingApiBridge(JNIEnv* env);

class SafeBrowsingApiHandlerBridge : public SafeBrowsingApiHandler {
 public:
  SafeBrowsingApiHandlerBridge();
  ~SafeBrowsingApiHandlerBridge() override;

  // Makes Native->Java call to check the URL against Safe Browsing lists.
  void StartURLCheck(const URLCheckCallbackMeta& callback,
                     const GURL& url,
                     const std::vector<SBThreatType>& threat_types) override;

 private:
  // Creates the j_api_handler_ if it hasn't been already.  If the API is not
  // supported, this will return false and j_api_handler_ will remain NULL.
  bool CheckApiIsSupported();

  // The Java-side SafeBrowsingApiHandler. Must call CheckApiIsSupported first.
  base::android::ScopedJavaLocalRef<jobject> j_api_handler_;

  // True if we've once tried to create the above object.
  bool checked_api_support_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingApiHandlerBridge);
};

}  // namespace safe_browsing
#endif  // CHROME_BROWSER_ANDROID_SAFE_BROWSING_SAFE_BROWSING_API_HANDLER_BRIDGE_H_
