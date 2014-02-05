// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_SERVICE_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_SERVICE_H_

#include "chrome/browser/web_resource/resource_request_allowed_notifier.h"

// Singleton managing the resources required for Translate.
class TranslateService : public ResourceRequestAllowedNotifier::Observer {
 public:
   // Must be called before the Translate feature can be used.
  static void Initialize();

  // Must be called to shut down the Translate feature.
  static void Shutdown(bool cleanup_pending_fetcher);

 private:
  TranslateService();
  ~TranslateService();

  // ResourceRequestAllowedNotifier::Observer methods.
  virtual void OnResourceRequestsAllowed() OVERRIDE;

  // Helper class to know if it's allowed to make network resource requests.
  ResourceRequestAllowedNotifier resource_request_allowed_notifier_;
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_SERVICE_H_
