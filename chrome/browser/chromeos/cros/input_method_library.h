// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_INPUT_METHOD_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_INPUT_METHOD_LIBRARY_H_
#pragma once

#include <string>
#include <utility>

#include "base/observer_list.h"
#include "base/time.h"
#include "base/timer.h"
#include "third_party/cros/chromeos_input_method.h"

namespace chromeos {

// This class handles the interaction with the ChromeOS language library APIs.
// Classes can add themselves as observers. Users can get an instance of this
// library class like this:
//   chromeos::CrosLibrary::Get()->GetInputMethodLibrary()
class InputMethodLibrary {
 public:
  class Observer {
   public:
    virtual ~Observer() = 0;
    // Called when the current input method is changed.
    virtual void InputMethodChanged(
        InputMethodLibrary* obj,
        const InputMethodDescriptor& previous_input_method,
        const InputMethodDescriptor& current_input_method,
        size_t num_active_input_methods) = 0;

    // Called when the active input methods are changed.
    virtual void ActiveInputMethodsChanged(
        InputMethodLibrary* obj,
        const InputMethodDescriptor& current_input_method,
        size_t num_active_input_methods) = 0;

    // Called when the preferences have to be updated.
    virtual void PreferenceUpdateNeeded(
        InputMethodLibrary* obj,
        const InputMethodDescriptor& previous_input_method,
        const InputMethodDescriptor& current_input_method) = 0;

    // Called by AddObserver() when the first observer is added.
    virtual void FirstObserverIsAdded(InputMethodLibrary* obj) = 0;
  };
  virtual ~InputMethodLibrary() {}

  // Adds an observer to receive notifications of input method related
  // changes as desribed in the Observer class above.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns the list of input methods we can select (i.e. active). If the cros
  // library is not found or IBus/DBus daemon is not alive, this function
  // returns a fallback input method list (and never returns NULL).
  virtual InputMethodDescriptors* GetActiveInputMethods() = 0;

  // Returns the number of active input methods.
  virtual size_t GetNumActiveInputMethods() = 0;

  // Returns the list of input methods we support, including ones not active.
  // If the cros library is not found or IBus/DBus daemon is not alive, this
  // function returns a fallback input method list (and never returns NULL).
  virtual InputMethodDescriptors* GetSupportedInputMethods() = 0;

  // Changes the current input method to |input_method_id|.
  virtual void ChangeInputMethod(const std::string& input_method_id) = 0;

  // Sets whether the input method property specified by |key| is activated. If
  // |activated| is true, activates the property. If |activate| is false,
  // deactivates the property. Examples of keys:
  // - "InputMode.Katakana"
  // - "InputMode.HalfWidthKatakana"
  // - "TypingMode.Romaji"
  // - "TypingMode.Kana"
  virtual void SetImePropertyActivated(const std::string& key,
                                       bool activated) = 0;

  // Returns true if the input method specified by |input_method_id| is active.
  virtual bool InputMethodIsActivated(const std::string& input_method_id) = 0;

  // Get a configuration of ibus-daemon or IBus engines and stores it on
  // |out_value|. Returns true if |out_value| is successfully updated.
  // When you would like to retrieve 'panel/custom_font', |section| should
  // be "panel", and |config_name| should be "custom_font".
  virtual bool GetImeConfig(const std::string& section,
                            const std::string& config_name,
                            ImeConfigValue* out_value) = 0;

  // Updates a configuration of ibus-daemon or IBus engines with |value|.
  // Returns true if the configuration (and all pending configurations, if any)
  // are processed. If ibus-daemon is not running, this function just queues
  // the request and returns false.
  // You can specify |section| and |config_name| arguments in the same way
  // as GetImeConfig() above.
  // Notice: This function might call the Observer::ActiveInputMethodsChanged()
  // callback function immediately, before returning from the SetImeConfig
  // function. See also http://crosbug.com/5217.
  virtual bool SetImeConfig(const std::string& section,
                            const std::string& config_name,
                            const ImeConfigValue& value) = 0;

  // Returns the keyboard overlay ID corresponding to |input_method_id|.
  // Returns an empty string if there is no corresponding keyboard overlay ID.
  virtual std::string GetKeyboardOverlayId(
      const std::string& input_method_id) = 0;

  // Sets the IME state to enabled, and launches input method daemon if needed.
  virtual void StartInputMethodDaemon() = 0;

  // Disables the IME, and kills the daemon process if they are running.
  virtual void StopInputMethodDaemon() = 0;

  // Controls whether the IME process is started when preload engines are
  // specificed, or defered until a non-default method is activated.
  virtual void SetDeferImeStartup(bool defer) = 0;

  // Controls whether the IME process is stopped when all non-default preload
  // engines are removed.
  virtual void SetEnableAutoImeShutdown(bool enable) = 0;


  virtual const InputMethodDescriptor& previous_input_method() const = 0;
  virtual const InputMethodDescriptor& current_input_method() const = 0;

  virtual const ImePropertyList& current_ime_properties() const = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via CrosLibrary::Get().
  static InputMethodLibrary* GetImpl(bool stub);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_INPUT_METHOD_LIBRARY_H_
