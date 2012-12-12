// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_BROWSER_STATE_MONITOR_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_BROWSER_STATE_MONITOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/prefs/public/pref_member.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_types.h"

namespace chromeos {
namespace input_method {

class InputMethodDelegate;

// A class which monitors a notification from the browser to keep track of the
// browser state (not logged in, logged in, etc.) and notify the current state
// to the input method manager. The class also updates the appropriate Chrome
// prefs via the InputMethodDelegate, depending on the current browser state.
class BrowserStateMonitor : public content::NotificationObserver,
                            public InputMethodManager::Observer {
 public:
  BrowserStateMonitor(InputMethodManager* manager,
                      InputMethodDelegate* delegate);
  virtual ~BrowserStateMonitor();

  InputMethodManager::State state() const { return state_; }

 protected:
  // InputMethodManager::Observer overrides:
  virtual void InputMethodChanged(InputMethodManager* manager,
                                  bool show_message) OVERRIDE;
  virtual void InputMethodPropertyChanged(InputMethodManager* manager) OVERRIDE;

  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  void SetState(InputMethodManager::State new_state);

  InputMethodManager* manager_;
  InputMethodDelegate* delegate_;
  InputMethodManager::State state_;

  // This is used to register this object to some browser notifications.
  content::NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(BrowserStateMonitor);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_BROWSER_STATE_MONITOR_H_
