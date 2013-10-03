// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/ibus_controller_impl.h"

#include <algorithm>  // for std::reverse.
#include <cstdio>
#include <cstring>  // for std::strcmp.
#include <set>
#include <sstream>
#include <stack>
#include <utility>

#include "ash/shell.h"
#include "base/bind.h"
#include "base/environment.h"
#include "base/files/file_path_watcher.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chromeos/ime/component_extension_ime_manager.h"
#include "chromeos/ime/extension_ime_util.h"
#include "chromeos/ime/ibus_bridge.h"
#include "chromeos/ime/ime_constants.h"
#include "chromeos/ime/input_method_config.h"
#include "chromeos/ime/input_method_property.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/base/ime/input_method_ibus.h"

namespace chromeos {
namespace input_method {

IBusControllerImpl::IBusControllerImpl() {
  IBusBridge::Get()->SetPropertyHandler(this);
}

IBusControllerImpl::~IBusControllerImpl() {
  IBusBridge::Get()->SetPropertyHandler(NULL);
}

bool IBusControllerImpl::ActivateInputMethodProperty(const std::string& key) {
  // The third parameter of ibus_input_context_property_activate() has to be
  // true when the |key| points to a radio button. false otherwise.
  bool found = false;
  for (size_t i = 0; i < current_property_list_.size(); ++i) {
    if (current_property_list_[i].key == key) {
      found = true;
      break;
    }
  }
  if (!found) {
    DVLOG(1) << "ActivateInputMethodProperty: unknown key: " << key;
    return false;
  }

  IBusEngineHandlerInterface* engine = IBusBridge::Get()->GetEngineHandler();
  if (engine)
    engine->PropertyActivate(key);
  return true;
}

void IBusControllerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void IBusControllerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

const InputMethodPropertyList&
IBusControllerImpl::GetCurrentProperties() const {
  return current_property_list_;
}

void IBusControllerImpl::ClearProperties() {
  current_property_list_.clear();
}

void IBusControllerImpl::RegisterProperties(
    const InputMethodPropertyList& property_list) {
  current_property_list_ = property_list;
  FOR_EACH_OBSERVER(IBusController::Observer, observers_, PropertyChanged());
}

}  // namespace input_method
}  // namespace chromeos
