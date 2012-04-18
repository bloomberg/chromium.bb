// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/ibus_controller_base.h"

namespace chromeos {
namespace input_method {

IBusControllerBase::IBusControllerBase() {
}

IBusControllerBase::~IBusControllerBase() {
}

void IBusControllerBase::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void IBusControllerBase::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool IBusControllerBase::SetInputMethodConfig(
    const std::string& section,
    const std::string& config_name,
    const InputMethodConfigValue& value) {
  DCHECK(!section.empty());
  DCHECK(!config_name.empty());

  const ConfigKeyType key(section, config_name);
  if (!SetInputMethodConfigInternal(key, value))
    return false;
  current_config_values_[key] = value;
  return true;
}

const InputMethodPropertyList&
IBusControllerBase::GetCurrentProperties() const {
  return current_property_list_;
}

bool IBusControllerBase::GetInputMethodConfigForTesting(
    const std::string& section,
    const std::string& config_name,
    InputMethodConfigValue* out_value) {
  DCHECK(out_value);
  const ConfigKeyType key(section, config_name);
  InputMethodConfigRequests::const_iterator iter =
      current_config_values_.find(key);
  if (iter == current_config_values_.end())
    return false;
  *out_value = iter->second;
  return true;
}

void IBusControllerBase::NotifyPropertyChangedForTesting() {
  FOR_EACH_OBSERVER(Observer, observers_, PropertyChanged());
}

void IBusControllerBase::SetCurrentPropertiesForTesting(
    const InputMethodPropertyList& current_property_list) {
  current_property_list_ = current_property_list;
}

}  // namespace input_method
}  // namespace chromeos
