// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_IBUS_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_IBUS_CONTROLLER_H_

#include "chrome/browser/chromeos/input_method/ibus_controller_base.h"

namespace chromeos {
namespace input_method {

struct InputMethodProperty;

// Mock IBusController implementation.
// TODO(nona): Remove this class and use MockIBus stuff instead.
class MockIBusController : public IBusControllerBase {
 public:
  MockIBusController();
  virtual ~MockIBusController();

  // IBusController overrides:
  virtual bool ActivateInputMethodProperty(const std::string& key) OVERRIDE;

  int activate_input_method_property_count_;
  std::string activate_input_method_property_key_;
  bool activate_input_method_property_return_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockIBusController);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_IBUS_CONTROLLER_H_
