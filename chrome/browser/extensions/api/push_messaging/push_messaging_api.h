// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PUSH_MESSAGING_PUSH_MESSAGING_API_H__
#define CHROME_BROWSER_EXTENSIONS_API_PUSH_MESSAGING_PUSH_MESSAGING_API_H__

#include <string>
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/extensions/api/push_messaging/invalidation_handler_observer.h"

class Profile;

namespace extensions {

// Observes a single InvalidationHandler and generates onMessage events.
class PushMessagingEventRouter : public InvalidationHandlerObserver {
 public:
  explicit PushMessagingEventRouter(Profile* profile);
  virtual ~PushMessagingEventRouter();

  void Init();

 private:
  FRIEND_TEST_ALL_PREFIXES(PushMessagingApiTest, EventDispatch);

  // InvalidationHandlerObserver implementation.
  virtual void OnMessage(const std::string& extension_id,
                         int subchannel,
                         const std::string& payload) OVERRIDE;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingEventRouter);
};

}  // namespace extension

#endif  // CHROME_BROWSER_EXTENSIONS_API_PUSH_MESSAGING_PUSH_MESSAGING_API_H__
