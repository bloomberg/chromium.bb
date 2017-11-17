// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Glue to pass Safe Browsing API requests between Chrome and GMSCore

#ifndef COMPONENTS_SAFE_BROWSING_ANDROID_SAFE_BROWSING_API_HANDLER_BRIDGE_H_
#define COMPONENTS_SAFE_BROWSING_ANDROID_SAFE_BROWSING_API_HANDLER_BRIDGE_H_

#include <jni.h>

#include <memory>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "components/safe_browsing/android/safe_browsing_api_handler.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"
#include "url/gurl.h"

namespace safe_browsing {
class SafeBrowsingApiHandlerBridge : public SafeBrowsingApiHandler {
 public:
  SafeBrowsingApiHandlerBridge();
  ~SafeBrowsingApiHandlerBridge() override;

  // Makes Native->Java call to check the URL against Safe Browsing lists.
  void StartURLCheck(const URLCheckCallbackMeta& callback,
                     const GURL& url,
                     const SBThreatTypeSet& threat_types) override;

 private:
  void Initialize();

  // Responsible for calling into Java from a separate sequence than the
  // SafeBrowsingApiHandlerBridge.
  class Core {
   public:
    Core();
    ~Core();

    void StartURLCheck(const URLCheckCallbackMeta& callback,
                       const GURL& url,
                       const SBThreatTypeSet& threat_types);

   private:
    // Creates the j_api_handler_ if it hasn't been already.  If the API is not
    // supported, this will return false and j_api_handler_ will remain NULL.
    bool CheckApiIsSupported();

    // The Java-side SafeBrowsingApiHandler. Must call CheckApiIsSupported
    // first.
    base::android::ScopedJavaGlobalRef<jobject> j_api_handler_;

    // True if we've once tried to create the above object.
    bool checked_api_support_ = false;

    SEQUENCE_CHECKER(sequence_checker_);
    DISALLOW_COPY_AND_ASSIGN(Core);
  };

  // nullptr if |this| and |core_| should live on the same sequence. Otherwise
  // it will be the sequence that |core_| lives on.
  scoped_refptr<base::SequencedTaskRunner> api_task_runner_;

  // Lives on |api_task_runner_|, unless DispatchSafetyNetCheckOffThread is
  // disabled.
  std::unique_ptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingApiHandlerBridge);
};

}  // namespace safe_browsing
#endif  // COMPONENTS_SAFE_BROWSING_ANDROID_SAFE_BROWSING_API_HANDLER_BRIDGE_H_
