// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LANGUAGE_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_LANGUAGE_LIBRARY_H_

#include <string>

#include "base/observer_list.h"
#include "base/singleton.h"
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
  };

  // This gets the singleton LanguageLibrary
  static LanguageLibrary* Get();

  // Makes sure the library is loaded, loading it if necessary. Returns true if
  // the library has been successfully loaded.
  static bool EnsureLoaded();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns the list of IMEs and keyboard layouts we can select
  // (i.e. active). If the cros library is not found or IBus/DBus daemon
  // is not alive, this function returns a fallback language list (and
  // never returns NULL).
  InputLanguageList* GetLanguages();

  // Returns the list of IMEs and keyboard layouts we support, including
  // ones not active. If the cros library is not found or IBus/DBus
  // daemon is not alive, this function returns a fallback language list
  // (and never returns NULL).
  InputLanguageList* GetSupportedLanguages();

  // Changes the current IME engine to |id| and enable IME (when |category|
  // is LANGUAGE_CATEGORY_IME). Changes the current XKB layout to |id| and
  // disable IME (when |category| is LANGUAGE_CATEGORY_XKB). |id| is a unique
  // identifier of a IME engine or XKB layout. Please check chromeos_language.h
  // in src third_party/cros/ for details.
  void ChangeLanguage(LanguageCategory category, const std::string& id);

  // Activates the language specified by |category| and |id|. Returns true
  // on success.
  bool ActivateLanguage(LanguageCategory category, const std::string& id);

  // Dectivates the language specified by |category| and |id|. Returns
  // true on success.
  bool DeactivateLanguage(LanguageCategory category, const std::string& id);

  const InputLanguage& current_language() const {
    return current_language_;
  }

 private:
  friend struct DefaultSingletonTraits<LanguageLibrary>;

  LanguageLibrary();
  ~LanguageLibrary();

  // This method is called when there's a change in language status.
  // This method is called on a background thread.
  static void LanguageChangedHandler(
      void* object, const InputLanguage& current_language);

  // This methods starts the monitoring of language changes.
  void Init();

  // Called by the handler to update the language status.
  // This will notify all the Observers.
  void UpdateCurrentLanguage(const InputLanguage& current_language);

  // A reference to the language api, to allow callbacks when the language
  // status changes.
  LanguageStatusConnection* language_status_connection_;
  ObserverList<Observer> observers_;

  // The language (IME or XKB layout) which currently selected.
  InputLanguage current_language_;

  DISALLOW_COPY_AND_ASSIGN(LanguageLibrary);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LANGUAGE_LIBRARY_H_

