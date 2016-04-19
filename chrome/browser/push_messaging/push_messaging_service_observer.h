// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_SERVICE_OBSERVER_H_
#define CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_SERVICE_OBSERVER_H_

#include <memory>

// Observes events and changes in the PushMessagingService.
class PushMessagingServiceObserver {
 public:
  // Creates a new PushMessagingServiceObserver.
  static std::unique_ptr<PushMessagingServiceObserver> Create();

  // Called when a push message has been fully handled.
  virtual void OnMessageHandled() = 0;
};

#endif  // CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_SERVICE_OBSERVER_H_
