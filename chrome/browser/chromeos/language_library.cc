// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/language_library.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros_library.h"

// Allows InvokeLater without adding refcounting. This class is a Singleton and
// won't be deleted until it's last InvokeLater is run.
template <>
struct RunnableMethodTraits<chromeos::LanguageLibrary> {
  void RetainCallee(chromeos::LanguageLibrary* obj) {}
  void ReleaseCallee(chromeos::LanguageLibrary* obj) {}
};

namespace {

// Finds a property which has |new_prop.key| from |prop_list|, and replaces the
// property with |new_prop|. Returns true if such a property is found.
bool FindAndUpdateProperty(const chromeos::ImeProperty& new_prop,
                           chromeos::ImePropertyList* prop_list) {
  for (size_t i = 0; i < prop_list->size(); ++i) {
    chromeos::ImeProperty& prop = prop_list->at(i);
    if (prop.key == new_prop.key) {
      const int saved_id = prop.selection_item_id;
      // Update the list except the radio id. As written in chromeos_language.h,
      // |prop.selection_item_id| is dummy.
      prop = new_prop;
      prop.selection_item_id = saved_id;
      return true;
    }
  }
  return false;
}

}  // namespace

namespace chromeos {

LanguageLibrary::LanguageLibrary() : language_status_connection_(NULL) {
}

LanguageLibrary::~LanguageLibrary() {
  if (EnsureLoadedAndStarted()) {
    chromeos::DisconnectLanguageStatus(language_status_connection_);
  }
}

LanguageLibrary::Observer::~Observer() {
}

// static
LanguageLibrary* LanguageLibrary::Get() {
  return Singleton<LanguageLibrary>::get();
}

void LanguageLibrary::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void LanguageLibrary::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

chromeos::InputLanguageList* LanguageLibrary::GetActiveLanguages() {
  chromeos::InputLanguageList* result = NULL;
  if (EnsureLoadedAndStarted()) {
    result = chromeos::GetActiveLanguages(language_status_connection_);
  }
  return result ? result : CreateFallbackInputLanguageList();
}

chromeos::InputLanguageList* LanguageLibrary::GetSupportedLanguages() {
  chromeos::InputLanguageList* result = NULL;
  if (EnsureLoadedAndStarted()) {
    result = chromeos::GetSupportedLanguages(language_status_connection_);
  }
  return result ? result : CreateFallbackInputLanguageList();
}

void LanguageLibrary::ChangeLanguage(
    LanguageCategory category, const std::string& id) {
  if (EnsureLoadedAndStarted()) {
    chromeos::ChangeLanguage(language_status_connection_, category, id.c_str());
  }
}

void LanguageLibrary::ActivateImeProperty(const std::string& key) {
  DCHECK(!key.empty());
  if (EnsureLoadedAndStarted()) {
    chromeos::ActivateImeProperty(
        language_status_connection_, key.c_str());
  }
}

void LanguageLibrary::DeactivateImeProperty(const std::string& key) {
  DCHECK(!key.empty());
  if (EnsureLoadedAndStarted()) {
    chromeos::DeactivateImeProperty(
        language_status_connection_, key.c_str());
  }
}

bool LanguageLibrary::ActivateLanguage(
    LanguageCategory category, const std::string& id) {
  bool success = false;
  if (EnsureLoadedAndStarted()) {
    success = chromeos::ActivateLanguage(language_status_connection_,
                                         category, id.c_str());
  }
  return success;
}

bool LanguageLibrary::DeactivateLanguage(
    LanguageCategory category, const std::string& id) {
  bool success = false;
  if (EnsureLoadedAndStarted()) {
    success = chromeos::DeactivateLanguage(language_status_connection_,
                                           category, id.c_str());
  }
  return success;
}

bool LanguageLibrary::GetImeConfig(
    const char* section, const char* config_name, ImeConfigValue* out_value) {
  bool success = false;
  if (EnsureLoadedAndStarted()) {
    success = chromeos::GetImeConfig(
        language_status_connection_, section, config_name, out_value);
  }
  return success;
}

bool LanguageLibrary::SetImeConfig(
    const char* section, const char* config_name, const ImeConfigValue& value) {
  bool success = false;
  if (EnsureLoadedAndStarted()) {
    success = chromeos::SetImeConfig(
        language_status_connection_, section, config_name, value);
  }
  return success;
}

// static
void LanguageLibrary::LanguageChangedHandler(
    void* object, const chromeos::InputLanguage& current_language) {
  LanguageLibrary* language_library = static_cast<LanguageLibrary*>(object);
  language_library->UpdateCurrentLanguage(current_language);
}

// static
void LanguageLibrary::RegisterPropertiesHandler(
    void* object, const ImePropertyList& prop_list) {
  LanguageLibrary* language_library = static_cast<LanguageLibrary*>(object);
  language_library->RegisterProperties(prop_list);
}

// static
void LanguageLibrary::UpdatePropertyHandler(
    void* object, const ImePropertyList& prop_list) {
  LanguageLibrary* language_library = static_cast<LanguageLibrary*>(object);
  language_library->UpdateProperty(prop_list);
}

bool LanguageLibrary::EnsureStarted() {
  if (language_status_connection_) {
    return true;
  }
  chromeos::LanguageStatusMonitorFunctions monitor_functions;
  monitor_functions.current_language = &LanguageChangedHandler;
  monitor_functions.register_ime_properties = &RegisterPropertiesHandler;
  monitor_functions.update_ime_property = &UpdatePropertyHandler;
  language_status_connection_
      = chromeos::MonitorLanguageStatus(monitor_functions, this);
  return language_status_connection_ != NULL;
}

bool LanguageLibrary::EnsureLoadedAndStarted() {
  return CrosLibrary::EnsureLoaded() && EnsureStarted();
}

void LanguageLibrary::UpdateCurrentLanguage(
    const chromeos::InputLanguage& current_language) {
  // Make sure we run on UI thread.
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    DLOG(INFO) << "UpdateCurrentLanguage (Background thread)";
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        // NewRunnableMethod() copies |current_language| by value.
        NewRunnableMethod(
            this, &LanguageLibrary::UpdateCurrentLanguage, current_language));
    return;
  }

  DLOG(INFO) << "UpdateCurrentLanguage (UI thread)";
  current_language_ = current_language;
  FOR_EACH_OBSERVER(Observer, observers_, LanguageChanged(this));
}

void LanguageLibrary::RegisterProperties(const ImePropertyList& prop_list) {
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(
            this, &LanguageLibrary::RegisterProperties, prop_list));
    return;
  }

  // |prop_list| might be empty. This means "clear all properties."
  current_ime_properties_ = prop_list;
  FOR_EACH_OBSERVER(Observer, observers_, ImePropertiesChanged(this));
}

void LanguageLibrary::UpdateProperty(const ImePropertyList& prop_list) {
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(
            this, &LanguageLibrary::UpdateProperty, prop_list));
    return;
  }

  for (size_t i = 0; i < prop_list.size(); ++i) {
    FindAndUpdateProperty(prop_list[i], &current_ime_properties_);
  }
  FOR_EACH_OBSERVER(Observer, observers_, ImePropertiesChanged(this));
}

}  // namespace chromeos
