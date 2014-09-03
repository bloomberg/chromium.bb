// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_TOUCH_VIEW_CONTROLLER_DELEGATE_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_TOUCH_VIEW_CONTROLLER_DELEGATE_H_

#include "ash/shell_observer.h"
#include "base/observer_list.h"

namespace chromeos {

// An interface for ash::MaximizeModeController.
class TouchViewControllerDelegate : public ash::ShellObserver {
 public:
  // Observer that reports changes to the state of MaximizeModeController's
  // rotation lock.
  class Observer {
   public:
    // Invoked when maximize mode started/ended.
    virtual void OnMaximizeModeStarted() {}
    virtual void OnMaximizeModeEnded() {}

   protected:
    virtual ~Observer() {}
  };

  TouchViewControllerDelegate();
  virtual ~TouchViewControllerDelegate();

  // Add/Remove observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Test if the Maximize Mode is enabled or not.
  bool IsMaximizeModeEnabled() const;


 private:
  // ash::ShellObserver implementation:
  virtual void OnMaximizeModeStarted() OVERRIDE;
  virtual void OnMaximizeModeEnded() OVERRIDE;

  // Mode state change observers.
  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(TouchViewControllerDelegate);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_TOUCH_VIEW_CONTROLLER_DELEGATE_H_
