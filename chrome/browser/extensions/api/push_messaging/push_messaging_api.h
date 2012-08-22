// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PUSH_MESSAGING_PUSH_MESSAGING_API_H__
#define CHROME_BROWSER_EXTENSIONS_API_PUSH_MESSAGING_PUSH_MESSAGING_API_H__

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/push_messaging/push_messaging_invalidation_handler_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace extensions {

class PushMessagingInvalidationMapper;

// Observes a single InvalidationHandler and generates onMessage events.
class PushMessagingEventRouter
    : public PushMessagingInvalidationHandlerDelegate,
      public content::NotificationObserver {
 public:
  explicit PushMessagingEventRouter(Profile* profile);
  virtual ~PushMessagingEventRouter();

  void Init();
  void Shutdown();

  PushMessagingInvalidationMapper* GetMapperForTest() const {
    return handler_.get();
  }
  void SetMapperForTest(scoped_ptr<PushMessagingInvalidationMapper> mapper);
  void TriggerMessageForTest(const std::string& extension_id,
                             int subchannel,
                             const std::string& payload);

 private:
  // InvalidationHandlerDelegate implementation.
  virtual void OnMessage(const std::string& extension_id,
                         int subchannel,
                         const std::string& payload) OVERRIDE;

  // content::NotificationDelegate implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  content::NotificationRegistrar registrar_;
  Profile* const profile_;
  scoped_ptr<PushMessagingInvalidationMapper> handler_;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingEventRouter);
};

}  // namespace extension

#endif  // CHROME_BROWSER_EXTENSIONS_API_PUSH_MESSAGING_PUSH_MESSAGING_API_H__
