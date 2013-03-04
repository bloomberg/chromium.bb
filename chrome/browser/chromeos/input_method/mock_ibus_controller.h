// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_IBUS_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_IBUS_CONTROLLER_H_

#include "chrome/browser/chromeos/input_method/ibus_controller_base.h"

namespace chromeos {
namespace input_method {

struct InputMethodConfigValue;
struct InputMethodProperty;

// Mock IBusController implementation.
class MockIBusController : public IBusControllerBase {
 public:
  MockIBusController();
  virtual ~MockIBusController();

  // IBusController overrides:
  virtual bool ChangeInputMethod(const std::string& id) OVERRIDE;
  virtual bool ActivateInputMethodProperty(const std::string& key) OVERRIDE;

  int change_input_method_count_;
  std::string change_input_method_id_;
  bool change_input_method_return_;
  int activate_input_method_property_count_;
  std::string activate_input_method_property_key_;
  bool activate_input_method_property_return_;
  int set_input_method_config_internal_count_;
  ConfigKeyType set_input_method_config_internal_key_;
  InputMethodConfigValue set_input_method_config_internal_value_;
  bool set_input_method_config_internal_return_;

 protected:
  // IBusControllerBase overrides:
  virtual bool SetInputMethodConfigInternal(
      const ConfigKeyType& key,
      const InputMethodConfigValue& value) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockIBusController);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_IBUS_CONTROLLER_H_
