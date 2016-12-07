// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_SETTINGS_APP_MONITOR_H_
#define CHROME_BROWSER_WIN_SETTINGS_APP_MONITOR_H_

#include <windows.h>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"

namespace shell_integration {
namespace win {

// A monitor that watches the Windows Settings app and notifies a delegate of
// particularly interesting events. An asychronous initialization procedure is
// started at instance creation, after which the delegate's |OnInitialized|
// method is invoked. Following successful initilization, the monitor is ready
// to observe the Settings app.
class SettingsAppMonitor {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Invoked to indicate that initialization is complete. |result| indicates
    // whether or not the operation succeeded and, if not, the reason for
    // failure.
    virtual void OnInitialized(HRESULT result) = 0;

    // Invoked when the Settings app has received keyboard focus.
    virtual void OnAppFocused() = 0;

    // Invoked when the user has invoked the Web browser chooser.
    virtual void OnChooserInvoked() = 0;

    // Invoked when the user has chosen a particular Web browser.
    virtual void OnBrowserChosen(const base::string16& browser_name) = 0;

    // Invoked when the Edge promo has received keyboard focus.
    virtual void OnPromoFocused() = 0;

    // Invoked when the user interacts with the Edge promo.
    virtual void OnPromoChoiceMade(bool accept_promo) = 0;
  };

  // |delegate| must outlive the monitor.
  explicit SettingsAppMonitor(Delegate* delegate);
  ~SettingsAppMonitor();

 private:
  class Context;

  // Methods for passing actions on to the instance's Delegate.
  void OnInitialized(HRESULT result);
  void OnAppFocused();
  void OnChooserInvoked();
  void OnBrowserChosen(const base::string16& browser_name);
  void OnPromoFocused();
  void OnPromoChoiceMade(bool accept_promo);

  base::ThreadChecker thread_checker_;
  Delegate* delegate_;

  // A thread in the COM MTA in which automation calls are made.
  base::Thread automation_thread_;

  // A pointer to the context object that lives on the automation thread.
  base::WeakPtr<Context> context_;

  // A factory for the weak pointer handed to |context_|, through which
  // notifications to the delegate are passed.
  base::WeakPtrFactory<SettingsAppMonitor> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SettingsAppMonitor);
};

}  // namespace win
}  // namespace shell_integration

#endif  // CHROME_BROWSER_WIN_SETTINGS_APP_MONITOR_H_
