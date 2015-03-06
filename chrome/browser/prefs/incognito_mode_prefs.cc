// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/incognito_mode_prefs.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_WIN)
#include <windows.h>
#include <wpcapi.h>
#include "base/win/scoped_comptr.h"
#include "base/win/windows_version.h"
#endif  // OS_WIN

#if defined(OS_ANDROID)
#include "chrome/browser/android/chromium_application.h"
#endif  // OS_ANDROID

using content::BrowserThread;

#if defined(OS_WIN)
namespace {

// Returns true if Windows Parental control activity logging is enabled. This
// feature is available on Windows 7 and beyond. This function should be called
// on a COM Initialized thread and is potentially blocking.
bool IsParentalControlActivityLoggingOn() {
  // Since we can potentially block, make sure the thread is okay with this.
  base::ThreadRestrictions::AssertIOAllowed();
  base::ThreadRestrictions::AssertWaitAllowed();

  // Query this info on Windows 7 and above.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return false;

  base::win::ScopedComPtr<IWindowsParentalControlsCore> parent_controls;
  HRESULT hr = parent_controls.CreateInstance(
      __uuidof(WindowsParentalControls));
  if (FAILED(hr))
    return false;

  base::win::ScopedComPtr<IWPCSettings> settings;
  hr = parent_controls->GetUserSettings(nullptr, settings.Receive());
  if (FAILED(hr))
    return false;

  unsigned long restrictions = 0;
  settings->GetRestrictions(&restrictions);

  return (restrictions & WPCFLAG_LOGGING_REQUIRED) == WPCFLAG_LOGGING_REQUIRED;
}

}  // namespace
#endif  // OS_WIN

// static
bool IncognitoModePrefs::IntToAvailability(int in_value,
                                           Availability* out_value) {
  if (in_value < 0 || in_value >= AVAILABILITY_NUM_TYPES) {
    *out_value = ENABLED;
    return false;
  }
  *out_value = static_cast<Availability>(in_value);
  return true;
}

// static
IncognitoModePrefs::Availability IncognitoModePrefs::GetAvailability(
    const PrefService* pref_service) {
  DCHECK(pref_service);
  int pref_value = pref_service->GetInteger(prefs::kIncognitoModeAvailability);
  Availability result = IncognitoModePrefs::ENABLED;
  bool valid = IntToAvailability(pref_value, &result);
  DCHECK(valid);
  if (ArePlatformParentalControlsEnabled()) {
    if (result == IncognitoModePrefs::FORCED)
      LOG(ERROR) << "Ignoring FORCED incognito. Parental control logging on";
    return IncognitoModePrefs::DISABLED;
  }
  return result;
}

// static
void IncognitoModePrefs::SetAvailability(PrefService* prefs,
                                         const Availability availability) {
  prefs->SetInteger(prefs::kIncognitoModeAvailability, availability);
}

// static
void IncognitoModePrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      prefs::kIncognitoModeAvailability,
      IncognitoModePrefs::ENABLED,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

// static
bool IncognitoModePrefs::ShouldLaunchIncognito(
    const base::CommandLine& command_line,
    const PrefService* prefs) {
  Availability incognito_avail = GetAvailability(prefs);
  return incognito_avail != IncognitoModePrefs::DISABLED &&
         (command_line.HasSwitch(switches::kIncognito) ||
          incognito_avail == IncognitoModePrefs::FORCED);
}

// static
bool IncognitoModePrefs::CanOpenBrowser(Profile* profile) {
  if (profile->IsGuestSession())
    return true;

  switch (GetAvailability(profile->GetPrefs())) {
    case IncognitoModePrefs::ENABLED:
      return true;

    case IncognitoModePrefs::DISABLED:
      return !profile->IsOffTheRecord();

    case IncognitoModePrefs::FORCED:
      return profile->IsOffTheRecord();

    default:
      NOTREACHED();
      return false;
  }
}

// static
bool IncognitoModePrefs::ArePlatformParentalControlsEnabled() {
#if defined(OS_WIN)
  enum class ParentalControlsState {
    UNKNOWN = 0,
    ACTIVITY_LOGGING_DISABLED = 1,
    ACTIVITY_LOGGING_ENABLED = 2,
  };
  static ParentalControlsState parental_controls_state =
      ParentalControlsState::UNKNOWN;
  if (parental_controls_state == ParentalControlsState::UNKNOWN) {
    // Production: The thread isn't initialized, so we're the only thread that
    // should be able to update this.
    // Test: The thread may be initialized, so check that it's the UI thread.
    DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
           BrowserThread::CurrentlyOn(BrowserThread::UI));
    parental_controls_state =
        IsParentalControlActivityLoggingOn() ?
            ParentalControlsState::ACTIVITY_LOGGING_ENABLED :
            ParentalControlsState::ACTIVITY_LOGGING_DISABLED;
  }
  return parental_controls_state ==
      ParentalControlsState::ACTIVITY_LOGGING_ENABLED;
#elif defined(OS_ANDROID)
  return chrome::android::ChromiumApplication::AreParentalControlsEnabled();
#else
  return false;
#endif
}

