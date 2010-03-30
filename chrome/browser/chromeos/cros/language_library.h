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
    virtual void LanguageChanged(LanguageLibrary* obj) = 0;
    virtual void ImePropertiesChanged(LanguageLibrary* obj) = 0;
  };
  virtual ~LanguageLibrary() {}

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns the list of IMEs and keyboard layouts we can select
  // (i.e. active). If the cros library is not found or IBus/DBus daemon
  // is not alive, this function returns a fallback language list (and
  // never returns NULL).
  virtual InputLanguageList* GetActiveLanguages() = 0;

  // Returns the list of IMEs and keyboard layouts we support, including
  // ones not active. If the cros library is not found or IBus/DBus
  // daemon is not alive, this function returns a fallback language list
  // (and never returns NULL).
  virtual InputLanguageList* GetSupportedLanguages() = 0;

  // Changes the current IME engine to |id| and enable IME (when |category|
  // is LANGUAGE_CATEGORY_IME). Changes the current XKB layout to |id| and
  // disable IME (when |category| is LANGUAGE_CATEGORY_XKB). |id| is a unique
  // identifier of a IME engine or XKB layout. Please check chromeos_language.h
  // in src third_party/cros/ for details.
  virtual void ChangeLanguage(LanguageCategory category,
                              const std::string& id) = 0;

  // Activates an IME property identified by |key|. Examples of keys are:
  // "InputMode.Katakana", "InputMode.HalfWidthKatakana", "TypingMode.Romaji",
  // and "TypingMode.Kana."
  virtual void ActivateImeProperty(const std::string& key) = 0;

  // Deactivates an IME property identified by |key|.
  virtual void DeactivateImeProperty(const std::string& key) = 0;

  // Sets whether the language specified by |category| and |id| is
  // activated. If |activated| is true, activates the language. If
  // |activate| is false, deactivates the language.
  virtual bool SetLanguageActivated(LanguageCategory category,
                                    const std::string& id,
                                    bool activated) = 0;

  // Returns true if the language specified by |category| and |id| is active.
  virtual bool LanguageIsActivated(LanguageCategory category,
                                   const std::string& id) = 0;

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

  virtual const InputLanguage& current_language() const = 0;

  virtual const ImePropertyList& current_ime_properties() const = 0;

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
  virtual InputLanguageList* GetActiveLanguages();
  virtual InputLanguageList* GetSupportedLanguages();
  virtual void ChangeLanguage(LanguageCategory category, const std::string& id);
  virtual void ActivateImeProperty(const std::string& key);
  virtual void DeactivateImeProperty(const std::string& key);
  virtual bool SetLanguageActivated(LanguageCategory category,
                                    const std::string& id,
                                    bool activated);
  virtual bool LanguageIsActivated(LanguageCategory category,
                                   const std::string& id);
  virtual bool GetImeConfig(
      const char* section, const char* config_name, ImeConfigValue* out_value);
  virtual bool SetImeConfig(const char* section,
                            const char* config_name,
                            const ImeConfigValue& value);

  virtual const InputLanguage& current_language() const {
    return current_language_;
  }

  virtual const ImePropertyList& current_ime_properties() const {
    return current_ime_properties_;
  }

 private:
  // This method is called when there's a change in language status.
  static void LanguageChangedHandler(
      void* object, const InputLanguage& current_language);

  // This method is called when an IME engine sends "RegisterProperties" signal.
  static void RegisterPropertiesHandler(
      void* object, const ImePropertyList& prop_list);

  // This method is called when an IME engine sends "UpdateProperty" signal.
  static void UpdatePropertyHandler(
      void* object, const ImePropertyList& prop_list);

  // Ensures that the monitoring of language changes is started. Starts
  // the monitoring if necessary. Returns true if the monitoring has been
  // successfully started.
  bool EnsureStarted();

  // Ensures that the cros library is loaded and the the monitoring is
  // started. Loads the cros library and starts the monitoring if
  // necessary.  Returns true if the two conditions are both met.
  bool EnsureLoadedAndStarted();

  // Called by the handler to update the language status.
  // This will notify all the Observers.
  void UpdateCurrentLanguage(const InputLanguage& current_language);

  // Called by the handler to register IME properties.
  void RegisterProperties(const ImePropertyList& prop_list);

  // Called by the handler to update IME properties.
  void UpdateProperty(const ImePropertyList& prop_list);

  // A reference to the language api, to allow callbacks when the language
  // status changes.
  LanguageStatusConnection* language_status_connection_;
  ObserverList<Observer> observers_;

  // The language (IME or XKB layout) which currently selected.
  InputLanguage current_language_;

  // The IME properties which the current IME engine uses. The list might be
  // empty when no IME is used.
  ImePropertyList current_ime_properties_;

  DISALLOW_COPY_AND_ASSIGN(LanguageLibraryImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_LANGUAGE_LIBRARY_H_

