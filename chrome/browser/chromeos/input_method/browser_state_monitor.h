// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_BROWSER_STATE_MONITOR_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_BROWSER_STATE_MONITOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "chromeos/ime/input_method_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace chromeos {
namespace input_method {

// Translates notifications from the browser (not logged in, logged in, etc.),
// into InputMethodManager::State transitions.
class BrowserStateMonitor : public content::NotificationObserver {
 public:
  // Constructs a monitor that will invoke the given observer callback whenever
  // the InputMethodManager::State changes. Assumes that the current state is
  // STATE_LOGIN_SCREEN. |observer| may be null.
  explicit BrowserStateMonitor(
      const base::Callback<void(InputMethodManager::State)>& observer);
  virtual ~BrowserStateMonitor();

  InputMethodManager::State state() const { return state_; }

  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  base::Callback<void(InputMethodManager::State)> observer_;
  InputMethodManager::State state_;
  content::NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(BrowserStateMonitor);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_BROWSER_STATE_MONITOR_H_
