// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_IBUS_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_IBUS_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/input_method/ibus_controller.h"
#include "chromeos/ime/input_method_config.h"

namespace chromeos {
namespace input_method {

// Mock IBusController implementation.
// TODO(nona): Remove this class and use MockIBus stuff instead.
class MockIBusController : public IBusController {
 public:
  MockIBusController();
  virtual ~MockIBusController();

  // IBusController overrides:
  virtual bool ActivateInputMethodProperty(const std::string& key) OVERRIDE;
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual const InputMethodPropertyList& GetCurrentProperties() const OVERRIDE;
  virtual void ClearProperties() OVERRIDE;

  // Notifies all |observers_|.
  void NotifyPropertyChangedForTesting();

  // Updates |current_property_list_|.
  void SetCurrentPropertiesForTesting(
    const InputMethodPropertyList& current_property_list);

  int activate_input_method_property_count_;
  std::string activate_input_method_property_key_;
  bool activate_input_method_property_return_;

 protected:
  ObserverList<Observer> observers_;

  // The value which will be returned by GetCurrentProperties(). Derived classes
  // should update this variable when needed.
  InputMethodPropertyList current_property_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockIBusController);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_IBUS_CONTROLLER_H_
