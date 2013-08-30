// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_CONTROLLER_IMPL_H_

#include <string>
#include <vector>

#include "base/observer_list.h"
#include "chrome/browser/chromeos/input_method/ibus_controller.h"
#include "chromeos/ime/ibus_bridge.h"
#include "chromeos/ime/input_method_property.h"

namespace chromeos {
namespace input_method {

struct InputMethodProperty;
typedef std::vector<InputMethodProperty> InputMethodPropertyList;

// The IBusController implementation.
class IBusControllerImpl : public IBusController,
                           public IBusPanelPropertyHandlerInterface {
 public:
  IBusControllerImpl();
  virtual ~IBusControllerImpl();

  // IBusController overrides:
  virtual bool ActivateInputMethodProperty(const std::string& key) OVERRIDE;

  // IBusController overrides. Derived classes should not override these 4
  // functions.
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual const InputMethodPropertyList& GetCurrentProperties() const OVERRIDE;
  virtual void ClearProperties() OVERRIDE;

  // Calls <anonymous_namespace>::FindAndUpdateProperty. This method is just for
  // unit testing.
  static bool FindAndUpdatePropertyForTesting(
      const InputMethodProperty& new_prop,
      InputMethodPropertyList* prop_list);

 protected:
  ObserverList<Observer> observers_;

  // The value which will be returned by GetCurrentProperties(). Derived classes
  // should update this variable when needed.
  InputMethodPropertyList current_property_list_;

 private:
  // IBusPanelPropertyHandlerInterface overrides:
  virtual void RegisterProperties(
      const IBusPropertyList& properties) OVERRIDE;
  virtual void UpdateProperty(const IBusProperty& property) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(IBusControllerImpl);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_CONTROLLER_IMPL_H_
