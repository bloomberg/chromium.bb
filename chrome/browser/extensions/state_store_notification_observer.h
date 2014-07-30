// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_STATE_STORE_NOTIFICATION_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_STATE_STORE_NOTIFICATION_OBSERVER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace extensions {
class StateStore;

// Initializes the StateStore when session restore is complete, for example when
// page load notifications are not sent ("Continue where I left off").
// http://crbug.com/230481
class StateStoreNotificationObserver : public content::NotificationObserver {
 public:
  explicit StateStoreNotificationObserver(StateStore* state_store);
  virtual ~StateStoreNotificationObserver();

  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  StateStore* state_store_;  // Not owned.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(StateStoreNotificationObserver);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_STATE_STORE_NOTIFICATION_OBSERVER_H_
