// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_AUTOMATION_CONTROLLER_H_
#define CHROME_BROWSER_WIN_AUTOMATION_CONTROLLER_H_

// Needed for <uiautomation.h>
#include <objbase.h>

#include <uiautomation.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"

// This is a helper class to facilitate the usage of the UI Automation API in
// the Chrome codebase. It takes care of initializing the Automation context and
// registering the event handlers. It also takes care of cleaning up the
// context on destruction.
//
// Users of this class should be careful because the delegate's functions are
// not invoked on the same sequence on which the AutomationController instance
// lives. This is because automation events arrives on the RPC thread, which is
// outside of the Task Scheduler's control.
class AutomationController {
 public:
  // The delegate is passed to the automation thread and the event handlers,
  // which runs in the context of a MTA.
  //
  // The call order is as follows:
  // - OnInitialized() is invoked on the automation thread.
  // If initialization succeeds:
  // - ConfigureCacheRequest() is invoked once per type of event.
  // - OnAutomationEvent() and OnFocusChangedEvent() are invoked as events
  //   happens.
  // - When the AutomationController instance is destroyed, the events
  //   automatically stop coming.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Invoked to indicate that initialization is complete. |result| indicates
    // whether or not the operation succeeded and, if not, the reason for
    // failure. On success, ConfigureCacheRequest() will be called once per
    // event handler (one for the automation events and one for the focus
    // changed events) and the delegate's other On*() methods will be invoked as
    // events are observed.
    virtual void OnInitialized(HRESULT result) const = 0;

    // Used to configure the event handlers so that the event sender element has
    // the required properties cached.
    // Runs on the context thread.
    virtual void ConfigureCacheRequest(
        IUIAutomationCacheRequest* cache_request) const = 0;

    // Invoked when an automation event happens.
    // This can be invoked on any thread in the automation MTA and so |this|
    // should be accessed carefully.
    virtual void OnAutomationEvent(IUIAutomation* automation,
                                   IUIAutomationElement* sender,
                                   EVENTID event_id) const = 0;

    // Invoked when a focus changed event happens.
    // This can be invoked on any thread in the automation MTA and so |this|
    // should be accessed carefully.
    virtual void OnFocusChangedEvent(IUIAutomation* automation,
                                     IUIAutomationElement* sender) const = 0;
  };

  explicit AutomationController(std::unique_ptr<Delegate> delegate);
  ~AutomationController();

 private:
  class Context;

  // A thread in the COM MTA in which automation calls are made.
  base::Thread automation_thread_;

  // A pointer to the context object that lives on the automation thread.
  base::WeakPtr<Context> context_;

  DISALLOW_COPY_AND_ASSIGN(AutomationController);
};

#endif  // CHROME_BROWSER_WIN_AUTOMATION_CONTROLLER_H_
