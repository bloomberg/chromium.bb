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

namespace chromeos {

LanguageLibrary::LanguageLibrary() : language_status_connection_(NULL) {
  if (EnsureLoaded()) {
    Init();
  }
}

LanguageLibrary::~LanguageLibrary() {
  if (EnsureLoaded()) {
    chromeos::DisconnectLanguageStatus(language_status_connection_);
  }
}

LanguageLibrary::Observer::~Observer() {
}

// static
LanguageLibrary* LanguageLibrary::Get() {
  return Singleton<LanguageLibrary>::get();
}

// static
bool LanguageLibrary::EnsureLoaded() {
  return CrosLibrary::EnsureLoaded();
}

void LanguageLibrary::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void LanguageLibrary::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

chromeos::InputLanguageList* LanguageLibrary::GetLanguages() {
  chromeos::InputLanguageList* result = NULL;
  if (EnsureLoaded()) {
    result = chromeos::GetLanguages(language_status_connection_);
  }
  return result ? result : CreateFallbackInputLanguageList();
}

void LanguageLibrary::ChangeLanguage(
    LanguageCategory category, const std::string& id) {
  if (EnsureLoaded()) {
    chromeos::ChangeLanguage(language_status_connection_, category, id.c_str());
  }
}

// static
void LanguageLibrary::LanguageChangedHandler(
    void* object, const chromeos::InputLanguage& current_language) {
  LanguageLibrary* language_library = static_cast<LanguageLibrary*>(object);
  language_library->UpdateCurrentLanguage(current_language);
}

void LanguageLibrary::Init() {
  language_status_connection_ = chromeos::MonitorLanguageStatus(
      &LanguageChangedHandler, this);
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

}  // namespace chromeos
