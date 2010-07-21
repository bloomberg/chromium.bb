// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/input_method_library.h"

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/keyboard_library.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "third_party/icu/public/common/unicode/uloc.h"

// Allows InvokeLater without adding refcounting. This class is a Singleton and
// won't be deleted until it's last InvokeLater is run.
DISABLE_RUNNABLE_METHOD_REFCOUNT(chromeos::InputMethodLibraryImpl);

namespace {

// Finds a property which has |new_prop.key| from |prop_list|, and replaces the
// property with |new_prop|. Returns true if such a property is found.
bool FindAndUpdateProperty(const chromeos::ImeProperty& new_prop,
                           chromeos::ImePropertyList* prop_list) {
  for (size_t i = 0; i < prop_list->size(); ++i) {
    chromeos::ImeProperty& prop = prop_list->at(i);
    if (prop.key == new_prop.key) {
      const int saved_id = prop.selection_item_id;
      // Update the list except the radio id. As written in
      // chromeos_input_method.h, |prop.selection_item_id| is dummy.
      prop = new_prop;
      prop.selection_item_id = saved_id;
      return true;
    }
  }
  return false;
}

// The default keyboard layout.
const char kDefaultKeyboardLayout[] = "us";

}  // namespace

namespace chromeos {

InputMethodLibraryImpl::InputMethodLibraryImpl()
    : input_method_status_connection_(NULL),
      previous_input_method_("", "", "", ""),
      current_input_method_("", "", "", "") {
  scoped_ptr<InputMethodDescriptors> input_method_descriptors(
      CreateFallbackInputMethodDescriptors());
  current_input_method_ = input_method_descriptors->at(0);
}

InputMethodLibraryImpl::~InputMethodLibraryImpl() {
  if (input_method_status_connection_) {
    chromeos::DisconnectInputMethodStatus(input_method_status_connection_);
  }
}

InputMethodLibraryImpl::Observer::~Observer() {
}

void InputMethodLibraryImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void InputMethodLibraryImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

chromeos::InputMethodDescriptors*
InputMethodLibraryImpl::GetActiveInputMethods() {
  chromeos::InputMethodDescriptors* result = NULL;
  // The connection does not need to be alive, but it does need to be created.
  if (EnsureLoadedAndStarted()) {
    result = chromeos::GetActiveInputMethods(input_method_status_connection_);
  }
  if (!result || result->empty()) {
    result = CreateFallbackInputMethodDescriptors();
  }
  return result;
}

size_t InputMethodLibraryImpl::GetNumActiveInputMethods() {
  scoped_ptr<InputMethodDescriptors> input_methods(GetActiveInputMethods());
  return input_methods->size();
}

chromeos::InputMethodDescriptors*
InputMethodLibraryImpl::GetSupportedInputMethods() {
  chromeos::InputMethodDescriptors* result = NULL;
  // The connection does not need to be alive, but it does need to be created.
  if (EnsureLoadedAndStarted()) {
    result = chromeos::GetSupportedInputMethods(
        input_method_status_connection_);
  }
  if (!result || result->empty()) {
    result = CreateFallbackInputMethodDescriptors();
  }
  return result;
}

void InputMethodLibraryImpl::ChangeInputMethod(
    const std::string& input_method_id) {
  if (EnsureLoadedAndStarted()) {
    chromeos::ChangeInputMethod(
        input_method_status_connection_, input_method_id.c_str());
  }
}

void InputMethodLibraryImpl::SetImePropertyActivated(const std::string& key,
                                                     bool activated) {
  DCHECK(!key.empty());
  if (EnsureLoadedAndStarted()) {
    chromeos::SetImePropertyActivated(
        input_method_status_connection_, key.c_str(), activated);
  }
}

bool InputMethodLibraryImpl::InputMethodIsActivated(
    const std::string& input_method_id) {
  scoped_ptr<InputMethodDescriptors> active_input_method_descriptors(
      CrosLibrary::Get()->GetInputMethodLibrary()->GetActiveInputMethods());
  for (size_t i = 0; i < active_input_method_descriptors->size(); ++i) {
    if (active_input_method_descriptors->at(i).id == input_method_id) {
      return true;
    }
  }
  return false;
}

bool InputMethodLibraryImpl::GetImeConfig(
    const char* section, const char* config_name, ImeConfigValue* out_value) {
  bool success = false;
  if (EnsureLoadedAndStarted()) {
    success = chromeos::GetImeConfig(
        input_method_status_connection_, section, config_name, out_value);
  }
  return success;
}

bool InputMethodLibraryImpl::SetImeConfig(
    const char* section, const char* config_name, const ImeConfigValue& value) {
  const ConfigKeyType key = std::make_pair(section, config_name);
  pending_config_requests_.erase(key);
  pending_config_requests_.insert(std::make_pair(key, value));
  current_config_values_[key] = value;
  FlushImeConfig();
  return pending_config_requests_.empty();
}

void InputMethodLibraryImpl::FlushImeConfig() {
  bool active_input_methods_are_changed = false;
  if (EnsureLoadedAndStarted()) {
    InputMethodConfigRequests::iterator iter = pending_config_requests_.begin();
    while (iter != pending_config_requests_.end()) {
      const std::string& section = iter->first.first;
      const std::string& config_name = iter->first.second;
      const ImeConfigValue& value = iter->second;
      if (chromeos::SetImeConfig(input_method_status_connection_,
                                 section.c_str(), config_name.c_str(), value)) {
        // Successfully sent. Remove the command and proceed to the next one.
        pending_config_requests_.erase(iter++);
        // Check if it's a change in active input methods.
        if (config_name == kPreloadEnginesConfigName) {
          active_input_methods_are_changed = true;
        }
      } else {
        LOG(ERROR) << "chromeos::SetImeConfig failed. Will retry later: "
                   << section << "/" << config_name;
        ++iter;  // Do not remove the command.
      }
    }
    if (pending_config_requests_.empty()) {
      timer_.Stop();  // no-op if it's not running.
    }
  } else {
    if (!timer_.IsRunning()) {
      static const int64 kTimerIntervalInSec = 1;
      timer_.Start(base::TimeDelta::FromSeconds(kTimerIntervalInSec), this,
                   &InputMethodLibraryImpl::FlushImeConfig);
    }
  }
  if (active_input_methods_are_changed) {
    FOR_EACH_OBSERVER(Observer, observers_, ActiveInputMethodsChanged(this));
  }
}

// static
void InputMethodLibraryImpl::InputMethodChangedHandler(
    void* object, const chromeos::InputMethodDescriptor& current_input_method) {
  InputMethodLibraryImpl* input_method_library =
      static_cast<InputMethodLibraryImpl*>(object);
  input_method_library->UpdateCurrentInputMethod(current_input_method);
}

// static
void InputMethodLibraryImpl::RegisterPropertiesHandler(
    void* object, const ImePropertyList& prop_list) {
  InputMethodLibraryImpl* input_method_library =
      static_cast<InputMethodLibraryImpl*>(object);
  input_method_library->RegisterProperties(prop_list);
}

// static
void InputMethodLibraryImpl::UpdatePropertyHandler(
    void* object, const ImePropertyList& prop_list) {
  InputMethodLibraryImpl* input_method_library =
      static_cast<InputMethodLibraryImpl*>(object);
  input_method_library->UpdateProperty(prop_list);
}

// static
void InputMethodLibraryImpl::ConnectionChangeHandler(void* object,
                                                     bool connected) {
  InputMethodLibraryImpl* input_method_library =
      static_cast<InputMethodLibraryImpl*>(object);
  if (connected) {
    input_method_library->pending_config_requests_.insert(
        input_method_library->current_config_values_.begin(),
        input_method_library->current_config_values_.end());
    input_method_library->FlushImeConfig();
  }
}

bool InputMethodLibraryImpl::EnsureStarted() {
  if (!input_method_status_connection_) {
    input_method_status_connection_ = chromeos::MonitorInputMethodStatus(
        this,
        &InputMethodChangedHandler,
        &RegisterPropertiesHandler,
        &UpdatePropertyHandler,
        &ConnectionChangeHandler);
  }
  return true;
}

bool InputMethodLibraryImpl::EnsureLoadedAndStarted() {
  return CrosLibrary::Get()->EnsureLoaded() &&
         EnsureStarted();
}

void InputMethodLibraryImpl::UpdateCurrentInputMethod(
    const chromeos::InputMethodDescriptor& new_input_method) {
  // Make sure we run on UI thread.
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    DLOG(INFO) << "UpdateCurrentInputMethod (Background thread)";
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        // NewRunnableMethod() copies |new_input_method| by value.
        NewRunnableMethod(
            this, &InputMethodLibraryImpl::UpdateCurrentInputMethod,
            new_input_method));
    return;
  }

  DLOG(INFO) << "UpdateCurrentInputMethod (UI thread)";
  // Change the keyboard layout to a preferred layout for the input method.
  CrosLibrary::Get()->GetKeyboardLibrary()->SetCurrentKeyboardLayoutByName(
      new_input_method.keyboard_layout);

  if (current_input_method_.id != new_input_method.id) {
    previous_input_method_ = current_input_method_;
    current_input_method_ = new_input_method;
  }
  FOR_EACH_OBSERVER(Observer, observers_, InputMethodChanged(this));
}

void InputMethodLibraryImpl::RegisterProperties(
    const ImePropertyList& prop_list) {
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(
            this, &InputMethodLibraryImpl::RegisterProperties, prop_list));
    return;
  }

  // |prop_list| might be empty. This means "clear all properties."
  current_ime_properties_ = prop_list;
  FOR_EACH_OBSERVER(Observer, observers_, ImePropertiesChanged(this));
}

void InputMethodLibraryImpl::UpdateProperty(const ImePropertyList& prop_list) {
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(
            this, &InputMethodLibraryImpl::UpdateProperty, prop_list));
    return;
  }

  for (size_t i = 0; i < prop_list.size(); ++i) {
    FindAndUpdateProperty(prop_list[i], &current_ime_properties_);
  }
  FOR_EACH_OBSERVER(Observer, observers_, ImePropertiesChanged(this));
}

}  // namespace chromeos
