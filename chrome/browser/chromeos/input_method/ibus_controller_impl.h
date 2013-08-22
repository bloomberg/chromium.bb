// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_CONTROLLER_IMPL_H_

#include <string>
#include <vector>

#include "chrome/browser/chromeos/input_method/ibus_controller_base.h"
#include "chromeos/ime/ibus_bridge.h"

namespace chromeos {
namespace input_method {

struct InputMethodProperty;
typedef std::vector<InputMethodProperty> InputMethodPropertyList;

// The IBusController implementation.
// TODO(nona): Merge to IBusControllerBase, there is no longer reason to split
//             this class into Impl and Base.
class IBusControllerImpl : public IBusControllerBase,
                           public IBusPanelPropertyHandlerInterface {
 public:
  IBusControllerImpl();
  virtual ~IBusControllerImpl();

  // IBusController overrides:
  virtual bool ActivateInputMethodProperty(const std::string& key) OVERRIDE;

  // Calls <anonymous_namespace>::FindAndUpdateProperty. This method is just for
  // unit testing.
  static bool FindAndUpdatePropertyForTesting(
      const InputMethodProperty& new_prop,
      InputMethodPropertyList* prop_list);

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
