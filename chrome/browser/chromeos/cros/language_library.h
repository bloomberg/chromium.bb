// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_LANGUAGE_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_LANGUAGE_LIBRARY_H_

#include <string>

#include "base/observer_list.h"
#include "base/time.h"
#include "third_party/cros/chromeos_language.h"

namespace chromeos {

// This class handles the interaction with the ChromeOS language library APIs.
// Classes can add themselves as observers. Users can get an instance of this
// library class like this: LanguageLibrary::Get()
class LanguageLibrary {
 public:
  class Observer {
   public:
    virtual ~Observer() = 0;
    virtual void InputMethodChanged(LanguageLibrary* obj) = 0;
    virtual void ImePropertiesChanged(LanguageLibrary* obj) = 0;
  };
  virtual ~LanguageLibrary() {}

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns the list of input methods we can select (i.e. active). If the cros
  // library is not found or IBus/DBus daemon is not alive, this function
  // returns a fallback input method list (and never returns NULL).
  virtual InputMethodDescriptors* GetActiveInputMethods() = 0;

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

  // Sets whether the input method specified by |input_method_id| is
  // activated. If |activated| is true, activates the input method. If
  // |activate| is false, deactivates the input method.
  virtual bool SetInputMethodActivated(const std::string& input_method_id,
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
  // Returns true if the configuration is successfully updated.
  // You can specify |section| and |config_name| arguments in the same way
  // as GetImeConfig() above.
  virtual bool SetImeConfig(const char* section,
                            const char* config_name,
                            const ImeConfigValue& value) = 0;

  virtual const InputMethodDescriptor& current_input_method() const = 0;

  virtual const ImePropertyList& current_ime_properties() const = 0;

  // Normalizes the language code and returns the normalized version.
  // The function concverts a two-letter language code to its
  // corresponding three-letter code like "ja" => "jpn". Otherwise,
  // returns the given language code as-is.
  static std::string NormalizeLanguageCode(const std::string& language_code);

  // Returns true if the given input method id is for a keyboard layout.
  static bool IsKeyboardLayout(const std::string& input_method_id);
};

// This class handles the interaction with the ChromeOS language library APIs.
// Classes can add themselves as observers. Users can get an instance of this
// library class like this: LanguageLibrary::Get()
class LanguageLibraryImpl : public LanguageLibrary {
 public:
  LanguageLibraryImpl();
  virtual ~LanguageLibraryImpl();

  // LanguageLibrary overrides.
  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);
  virtual InputMethodDescriptors* GetActiveInputMethods();
  virtual InputMethodDescriptors* GetSupportedInputMethods();
  virtual void ChangeInputMethod(const std::string& input_method_id);
  virtual void SetImePropertyActivated(const std::string& key,
                                       bool activated);
  virtual bool SetInputMethodActivated(const std::string& input_method_id,
                                       bool activated);
  virtual bool InputMethodIsActivated(const std::string& input_method_id);
  virtual bool GetImeConfig(
      const char* section, const char* config_name, ImeConfigValue* out_value);
  virtual bool SetImeConfig(const char* section,
                            const char* config_name,
                            const ImeConfigValue& value);

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

  // A reference to the language api, to allow callbacks when the input method
  // status changes.
  LanguageStatusConnection* language_status_connection_;
  ObserverList<Observer> observers_;

  // The input method which is currently selected.
  InputMethodDescriptor current_input_method_;

  // The input method properties which the current input method uses. The list
  // might be empty when no input method is used.
  ImePropertyList current_ime_properties_;

  DISALLOW_COPY_AND_ASSIGN(LanguageLibraryImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_LANGUAGE_LIBRARY_H_

