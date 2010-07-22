// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_INPUT_METHOD_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_INPUT_METHOD_LIBRARY_H_

#include <map>
#include <string>
#include <utility>

#include "base/observer_list.h"
#include "base/time.h"
#include "base/timer.h"
#include "cros/chromeos_input_method.h"

namespace chromeos {

// This class handles the interaction with the ChromeOS language library APIs.
// Classes can add themselves as observers. Users can get an instance of this
// library class like this: InputMethodLibrary::Get()
class InputMethodLibrary {
 public:
  class Observer {
   public:
    virtual ~Observer() = 0;
    virtual void InputMethodChanged(InputMethodLibrary* obj) = 0;
    virtual void ImePropertiesChanged(InputMethodLibrary* obj) = 0;
    virtual void ActiveInputMethodsChanged(InputMethodLibrary* obj) = 0;
  };
  virtual ~InputMethodLibrary() {}

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
  virtual bool GetImeConfig(
      const char* section, const char* config_name,
      ImeConfigValue* out_value) = 0;

  // Updates a configuration of ibus-daemon or IBus engines with |value|.
  // Returns true if the configuration (and all pending configurations, if any)
  // are processed. If ibus-daemon is not running, this function just queues
  // the request and returns false.
  // You can specify |section| and |config_name| arguments in the same way
  // as GetImeConfig() above.
  virtual bool SetImeConfig(const char* section,
                            const char* config_name,
                            const ImeConfigValue& value) = 0;

  virtual const InputMethodDescriptor& previous_input_method() const = 0;
  virtual const InputMethodDescriptor& current_input_method() const = 0;

  virtual const ImePropertyList& current_ime_properties() const = 0;
};

// This class handles the interaction with the ChromeOS language library APIs.
// Classes can add themselves as observers. Users can get an instance of this
// library class like this: InputMethodLibrary::Get()
class InputMethodLibraryImpl : public InputMethodLibrary {
 public:
  InputMethodLibraryImpl();
  virtual ~InputMethodLibraryImpl();

  // InputMethodLibrary overrides.
  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);
  virtual InputMethodDescriptors* GetActiveInputMethods();
  virtual size_t GetNumActiveInputMethods();
  virtual InputMethodDescriptors* GetSupportedInputMethods();
  virtual void ChangeInputMethod(const std::string& input_method_id);
  virtual void SetImePropertyActivated(const std::string& key,
                                       bool activated);
  virtual bool InputMethodIsActivated(const std::string& input_method_id);
  virtual bool GetImeConfig(
      const char* section, const char* config_name, ImeConfigValue* out_value);
  virtual bool SetImeConfig(const char* section,
                            const char* config_name,
                            const ImeConfigValue& value);

  virtual const InputMethodDescriptor& previous_input_method() const {
    return previous_input_method_;
  }
  virtual const InputMethodDescriptor& current_input_method() const {
    return current_input_method_;
  }

  virtual const ImePropertyList& current_ime_properties() const {
    return current_ime_properties_;
  }

 private:
  // This method is called when there's a change in input method status.
  static void InputMethodChangedHandler(
      void* object, const InputMethodDescriptor& current_input_method);

  // This method is called when an input method sends "RegisterProperties"
  // signal.
  static void RegisterPropertiesHandler(
      void* object, const ImePropertyList& prop_list);

  // This method is called when an input method sends "UpdateProperty" signal.
  static void UpdatePropertyHandler(
      void* object, const ImePropertyList& prop_list);

  // This method is called when bus connects or disconnects.
  static void ConnectionChangeHandler(void* object, bool connected);

  // Ensures that the monitoring of input method changes is started. Starts
  // the monitoring if necessary. Returns true if the monitoring has been
  // successfully started.
  bool EnsureStarted();

  // Ensures that the cros library is loaded and the the monitoring is
  // started. Loads the cros library and starts the monitoring if
  // necessary.  Returns true if the two conditions are both met.
  bool EnsureLoadedAndStarted();

  // Called by the handler to update the input method status.
  // This will notify all the Observers.
  void UpdateCurrentInputMethod(
      const InputMethodDescriptor& current_input_method);

  // Called by the handler to register input method properties.
  void RegisterProperties(const ImePropertyList& prop_list);

  // Called by the handler to update input method properties.
  void UpdateProperty(const ImePropertyList& prop_list);

  // Tries to send all pending SetImeConfig requests to the input method config
  // daemon.
  void FlushImeConfig();

  // A reference to the language api, to allow callbacks when the input method
  // status changes.
  InputMethodStatusConnection* input_method_status_connection_;
  ObserverList<Observer> observers_;

  // The input method which was/is selected.
  InputMethodDescriptor previous_input_method_;
  InputMethodDescriptor current_input_method_;

  // The input method properties which the current input method uses. The list
  // might be empty when no input method is used.
  ImePropertyList current_ime_properties_;

  typedef std::pair<std::string, std::string> ConfigKeyType;
  typedef std::map<ConfigKeyType, ImeConfigValue> InputMethodConfigRequests;
  // SetImeConfig requests that are not yet completed.
  // Use a map to queue config requests, so we only send the last request for
  // the same config key (i.e. we'll discard ealier requests for the same
  // config key). As we discard old requests for the same config key, the order
  // of requests doesn't matter, so it's safe to use a map.
  InputMethodConfigRequests pending_config_requests_;

  // Values that have been set via SetImeConfig().  We keep a copy available to
  // resend if the ime restarts and loses its state.
  InputMethodConfigRequests current_config_values_;

  // A timer for retrying to send |pendning_config_commands_| to the input
  // method config daemon.
  base::OneShotTimer<InputMethodLibraryImpl> timer_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodLibraryImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_INPUT_METHOD_LIBRARY_H_
