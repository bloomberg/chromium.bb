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
#include "base/message_loop.h"
#include "base/rand_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_split.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/ibus/ibus_client.h"
#include "chromeos/dbus/ibus/ibus_config_client.h"
#include "chromeos/dbus/ibus/ibus_constants.h"
#include "chromeos/dbus/ibus/ibus_input_context_client.h"
#include "chromeos/dbus/ibus/ibus_panel_service.h"
#include "chromeos/dbus/ibus/ibus_property.h"
#include "chromeos/ime/input_method_config.h"
#include "chromeos/ime/input_method_property.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/base/ime/input_method_ibus.h"

namespace {

// Finds a property which has |new_prop.key| from |prop_list|, and replaces the
// property with |new_prop|. Returns true if such a property is found.
bool FindAndUpdateProperty(
    const chromeos::input_method::InputMethodProperty& new_prop,
    chromeos::input_method::InputMethodPropertyList* prop_list) {
  for (size_t i = 0; i < prop_list->size(); ++i) {
    chromeos::input_method::InputMethodProperty& prop = prop_list->at(i);
    if (prop.key == new_prop.key) {
      prop = new_prop;
      return true;
    }
  }
  return false;
}

void ConfigSetValueErrorCallback() {
  DVLOG(1) << "IBusConfig: SetValue is failed.";
}

}  // namespace

namespace chromeos {
namespace input_method {

namespace {

// Returns true if |key| is blacklisted.
bool PropertyKeyIsBlacklisted(const std::string& key) {
  // The list of input method property keys that we don't handle.
  static const char* kInputMethodPropertyKeysBlacklist[] = {
    "setup",  // used in ibus-m17n.
    "status",  // used in ibus-m17n.
  };
  for (size_t i = 0; i < arraysize(kInputMethodPropertyKeysBlacklist); ++i) {
    if (key == kInputMethodPropertyKeysBlacklist[i])
      return true;
  }
  return false;
}

// This function is called by and FlattenProperty() and converts IBus
// representation of a property, |ibus_prop|, to our own and push_back the
// result to |out_prop_list|. This function returns true on success, and
// returns false if sanity checks for |ibus_prop| fail.
bool ConvertProperty(const IBusProperty& ibus_prop,
                     InputMethodPropertyList* out_prop_list) {
  DCHECK(out_prop_list);
  DCHECK(!ibus_prop.key().empty());
  IBusProperty::IBusPropertyType type = ibus_prop.type();

  // Sanity checks.
  const bool has_sub_props = !ibus_prop.sub_properties().empty();
  if (has_sub_props && (type != IBusProperty::IBUS_PROPERTY_TYPE_MENU)) {
    DVLOG(1) << "The property has sub properties, "
             << "but the type of the property is not PROP_TYPE_MENU";
    return false;
  }
  if ((!has_sub_props) &&
      (type == IBusProperty::IBUS_PROPERTY_TYPE_MENU)) {
    // This is usually not an error. ibus-daemon sometimes sends empty props.
    DVLOG(1) << "Property list is empty";
    return false;
  }
  if (type == IBusProperty::IBUS_PROPERTY_TYPE_SEPARATOR ||
      type == IBusProperty::IBUS_PROPERTY_TYPE_MENU) {
    // This is not an error, but we don't push an item for these types.
    return true;
  }

  // This label will be localized later.
  // See chrome/browser/chromeos/input_method/input_method_util.cc.
  std::string label_to_use;
  if (!ibus_prop.tooltip().empty())
    label_to_use = ibus_prop.tooltip();
  else if (!ibus_prop.label().empty())
    label_to_use = ibus_prop.label();
  else
    label_to_use = ibus_prop.key();

  out_prop_list->push_back(InputMethodProperty(
      ibus_prop.key(),
      label_to_use,
      (type == IBusProperty::IBUS_PROPERTY_TYPE_RADIO),
      ibus_prop.checked()));
  return true;
}

// Converts |ibus_prop| to |out_prop_list|. Please note that |ibus_prop|
// may or may not have children. See the comment for FlattenPropertyList
// for details. Returns true if no error is found.
bool FlattenProperty(const IBusProperty& ibus_prop,
                     InputMethodPropertyList* out_prop_list) {
  DCHECK(out_prop_list);

  // Filter out unnecessary properties.
  if (PropertyKeyIsBlacklisted(ibus_prop.key()))
    return true;

  // Convert |prop| to InputMethodProperty and push it to |out_prop_list|.
  if (!ConvertProperty(ibus_prop, out_prop_list))
    return false;

  // Process childrens iteratively (if any). Push all sub properties to the
  // stack.
  if (!ibus_prop.sub_properties().empty()) {
    const IBusPropertyList& sub_props = ibus_prop.sub_properties();
    for (size_t i = 0; i < sub_props.size(); ++i) {
      if (!FlattenProperty(*sub_props[i], out_prop_list))
        return false;
    }
  }
  return true;
}

// Converts IBus representation of a property list, |ibus_prop_list| to our
// own. This function also flatten the original list (actually it's a tree).
// Returns true if no error is found. The conversion to our own type is
// necessary since our language switcher in Chrome tree don't (or can't) know
// IBus types. Here is an example:
//
// ======================================================================
// Input:
//
// --- Item-1
//  |- Item-2
//  |- SubMenuRoot --- Item-3-1
//  |               |- Item-3-2
//  |               |- Item-3-3
//  |- Item-4
//
// (Note: Item-3-X is a selection item since they're on a sub menu.)
//
// Output:
//
// Item-1, Item-2, Item-3-1, Item-3-2, Item-3-3, Item-4
// (Note: SubMenuRoot does not appear in the output.)
// ======================================================================
bool FlattenPropertyList(const IBusPropertyList& ibus_prop_list,
                         InputMethodPropertyList* out_prop_list) {
  DCHECK(out_prop_list);

  bool result = true;
  for (size_t i = 0; i < ibus_prop_list.size(); ++i) {
    result &= FlattenProperty(*ibus_prop_list[i], out_prop_list);
  }
  return result;
}

}  // namespace

IBusControllerImpl::IBusControllerImpl()
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  IBusDaemonController::GetInstance()->AddObserver(this);
}

IBusControllerImpl::~IBusControllerImpl() {
  IBusDaemonController::GetInstance()->RemoveObserver(this);
}

bool IBusControllerImpl::ChangeInputMethod(const std::string& id) {
  // Sanity checks.
  DCHECK(!InputMethodUtil::IsKeyboardLayout(id));
  if (!whitelist_.InputMethodIdIsWhitelisted(id) &&
      !InputMethodUtil::IsExtensionInputMethod(id))
    return false;

  // Clear input method properties unconditionally if |id| is not equal to
  // |current_input_method_id_|.
  //
  // When switching to another input method and no text area is focused,
  // RegisterProperties signal for the new input method will NOT be sent
  // until a text area is focused. Therefore, we have to clear the old input
  // method properties here to keep the input method switcher status
  // consistent.
  //
  // When |id| and |current_input_method_id_| are the same, the properties
  // shouldn't be cleared. If we do that, something wrong happens in step #4
  // below:
  // 1. Enable "xkb:us::eng" and "mozc". Switch to "mozc".
  // 2. Focus Omnibox. IME properties for mozc are sent to Chrome.
  // 3. Switch to "xkb:us::eng". No function in this file is called.
  // 4. Switch back to "mozc". ChangeInputMethod("mozc") is called, but it's
  //    basically NOP since ibus-daemon's current IME is already "mozc".
  //    IME properties are not sent to Chrome for the same reason.
  if (id != current_input_method_id_) {
    const IBusPropertyList empty_list;
    RegisterProperties(empty_list);
  }

  current_input_method_id_ = id;

  if (!IBusConnectionsAreAlive()) {
    DVLOG(1) << "ChangeInputMethod: IBus connection is not alive (yet).";
    // |id| will become usable shortly since Start() has already been called.
    // Just return true.
  } else {
    SendChangeInputMethodRequest(id);
  }

  return true;
}

bool IBusControllerImpl::ActivateInputMethodProperty(const std::string& key) {
  if (!IBusConnectionsAreAlive()) {
    DVLOG(1) << "ActivateInputMethodProperty: IBus connection is not alive";
    return false;
  }

  // The third parameter of ibus_input_context_property_activate() has to be
  // true when the |key| points to a radio button. false otherwise.
  bool is_radio = true;
  size_t i;
  for (i = 0; i < current_property_list_.size(); ++i) {
    if (current_property_list_[i].key == key) {
      is_radio = current_property_list_[i].is_selection_item;
      break;
    }
  }
  if (i == current_property_list_.size()) {
    DVLOG(1) << "ActivateInputMethodProperty: unknown key: " << key;
    return false;
  }

  IBusInputContextClient* client
      = DBusThreadManager::Get()->GetIBusInputContextClient();
  if (client)
    client->PropertyActivate(key,
                             static_cast<ibus::IBusPropertyState>(is_radio));
  return true;
}

bool IBusControllerImpl::IBusConnectionsAreAlive() {
  return DBusThreadManager::Get() &&
      DBusThreadManager::Get()->GetIBusBus() != NULL;
}

void IBusControllerImpl::SendChangeInputMethodRequest(const std::string& id) {
  // Change the global engine *asynchronously*.
  IBusClient* client = DBusThreadManager::Get()->GetIBusClient();
  if (client)
    client->SetGlobalEngine(id.c_str(), base::Bind(&base::DoNothing));
}

bool IBusControllerImpl::SetInputMethodConfigInternal(
    const ConfigKeyType& key,
    const InputMethodConfigValue& value) {
  if (value.type != InputMethodConfigValue::kValueTypeString &&
      value.type != InputMethodConfigValue::kValueTypeInt &&
      value.type != InputMethodConfigValue::kValueTypeBool &&
      value.type != InputMethodConfigValue::kValueTypeStringList) {
    DVLOG(1) << "SendInputMethodConfig: unknown value.type";
    return false;
  }

  IBusConfigClient* client = DBusThreadManager::Get()->GetIBusConfigClient();
  if (!client) {
    // Should return true if the ibus-memconf is not ready to use, otherwise IME
    // configuration will not be initialized.
    return true;
  }

  switch (value.type) {
    case InputMethodConfigValue::kValueTypeString:
      client->SetStringValue(key.first,
                             key.second,
                             value.string_value,
                             base::Bind(&ConfigSetValueErrorCallback));
      return true;
    case InputMethodConfigValue::kValueTypeInt:
      client->SetIntValue(key.first,
                          key.second,
                          value.int_value,
                          base::Bind(&ConfigSetValueErrorCallback));
      return true;
    case InputMethodConfigValue::kValueTypeBool:
      client->SetBoolValue(key.first,
                           key.second,
                           value.bool_value,
                           base::Bind(&ConfigSetValueErrorCallback));
      return true;
    case InputMethodConfigValue::kValueTypeStringList:
      client->SetStringListValue(key.first,
                                 key.second,
                                 value.string_list_value,
                                 base::Bind(&ConfigSetValueErrorCallback));
      return true;
    default:
      NOTREACHED() << "SendInputMethodConfig: unknown value.type";
      return false;
  }
}

void IBusControllerImpl::RegisterProperties(
    const IBusPropertyList& ibus_prop_list) {
  // Note: |panel| can be NULL. See ChangeInputMethod().
  current_property_list_.clear();
  if (!FlattenPropertyList(ibus_prop_list, &current_property_list_))
    current_property_list_.clear(); // Clear properties on errors.
  FOR_EACH_OBSERVER(IBusController::Observer, observers_, PropertyChanged());
}

void IBusControllerImpl::UpdateProperty(const IBusProperty& ibus_prop) {
  InputMethodPropertyList prop_list;  // our representation.
  if (!FlattenProperty(ibus_prop, &prop_list)) {
    // Don't update the UI on errors.
    DVLOG(1) << "Malformed properties are detected";
    return;
  }

  // Notify the change.
  if (!prop_list.empty()) {
    for (size_t i = 0; i < prop_list.size(); ++i) {
      FindAndUpdateProperty(prop_list[i], &current_property_list_);
    }
    FOR_EACH_OBSERVER(IBusController::Observer, observers_, PropertyChanged());
  }
}

void IBusControllerImpl::OnIBusConfigClientInitialized() {
  DCHECK(thread_checker_.CalledOnValidThread());
  InputMethodConfigRequests::const_iterator iter =
      current_config_values_.begin();
  for (; iter != current_config_values_.end(); ++iter) {
    SetInputMethodConfigInternal(iter->first, iter->second);
  }
}

void IBusControllerImpl::OnConnected() {
  DCHECK(thread_checker_.CalledOnValidThread());

  DBusThreadManager::Get()->GetIBusPanelService()->SetUpPropertyHandler(this);

  // Restore previous input method at the beggining of connection.
  if (!current_input_method_id_.empty())
    SendChangeInputMethodRequest(current_input_method_id_);

  DBusThreadManager::Get()->GetIBusConfigClient()->InitializeAsync(
      base::Bind(&IBusControllerImpl::OnIBusConfigClientInitialized,
                 weak_ptr_factory_.GetWeakPtr()));
}

void IBusControllerImpl::OnDisconnected() {
  DBusThreadManager::Get()->GetIBusPanelService()->SetUpPropertyHandler(NULL);
}

// static
bool IBusControllerImpl::FindAndUpdatePropertyForTesting(
    const chromeos::input_method::InputMethodProperty& new_prop,
    chromeos::input_method::InputMethodPropertyList* prop_list) {
  return FindAndUpdateProperty(new_prop, prop_list);
}

}  // namespace input_method
}  // namespace chromeos
