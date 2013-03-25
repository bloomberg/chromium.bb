// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_CONTROLLER_BASE_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_CONTROLLER_BASE_H_

#include <map>
#include <utility>

#include "base/observer_list.h"
#include "chrome/browser/chromeos/input_method/ibus_controller.h"
#include "chromeos/ime/input_method_config.h"
#include "chromeos/ime/input_method_property.h"

namespace chromeos {
namespace input_method {

// The common implementation of the IBusController. This file does not depend on
// libibus, hence is unit-testable.
class IBusControllerBase : public IBusController {
 public:
  IBusControllerBase();
  virtual ~IBusControllerBase();

  // IBusController overrides. Derived classes should not override these 4
  // functions.
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual bool SetInputMethodConfig(
      const std::string& section,
      const std::string& config_name,
      const InputMethodConfigValue& value) OVERRIDE;
  virtual const InputMethodPropertyList& GetCurrentProperties() const OVERRIDE;

  // Gets the current input method configuration.
  bool GetInputMethodConfigForTesting(const std::string& section,
                                      const std::string& config_name,
                                      InputMethodConfigValue* out_value);

  // Notifies all |observers_|.
  void NotifyPropertyChangedForTesting();

  // Updates |current_property_list_|.
  void SetCurrentPropertiesForTesting(
      const InputMethodPropertyList& current_property_list);

 protected:
  typedef std::pair<std::string, std::string> ConfigKeyType;
  typedef std::map<
    ConfigKeyType, InputMethodConfigValue> InputMethodConfigRequests;

  virtual bool SetInputMethodConfigInternal(
      const ConfigKeyType& key,
      const InputMethodConfigValue& value) = 0;

  ObserverList<Observer> observers_;

  // Values that have been set via SetInputMethodConfig(). We keep a copy
  // available to (re)send when the system input method framework (re)starts.
  InputMethodConfigRequests current_config_values_;

  // The value which will be returned by GetCurrentProperties(). Derived classes
  // should update this variable when needed.
  InputMethodPropertyList current_property_list_;

  DISALLOW_COPY_AND_ASSIGN(IBusControllerBase);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_CONTROLLER_BASE_H_
