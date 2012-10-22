// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTENTS_INTENT_SERVICE_HOST_H_
#define CHROME_BROWSER_INTENTS_INTENT_SERVICE_HOST_H_

class GURL;

namespace content {
class WebIntentsDispatcher;
}

namespace web_intents {

// Interface allowing services to be implemented in various host environments.
class IntentServiceHost {
 public:
  virtual ~IntentServiceHost() {}

  // Handle all necessary service-side processing of an intent. A service
  // can reply to the intent via |dispatcher|.
  virtual void HandleIntent(content::WebIntentsDispatcher* dispatcher) = 0;
};

}  // namespace web_intents

#endif  // CHROME_BROWSER_INTENTS_INTENT_SERVICE_HOST_H_
