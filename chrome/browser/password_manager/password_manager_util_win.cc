// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// windows.h must be first otherwise Win8 SDK breaks.
#include <windows.h>
#include <wincred.h>
#include <LM.h>

// SECURITY_WIN32 must be defined in order to get
// EXTENDED_NAME_FORMAT enumeration.
#define SECURITY_WIN32 1
#include <security.h>
#undef SECURITY_WIN32

#include "chrome/browser/password_manager/password_manager_util.h"

#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#endif

// static
void password_manager::PasswordManager::RegisterLocalPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(password_manager::prefs::kOsPasswordLastChanged,
                              0);
  registry->RegisterBooleanPref(password_manager::prefs::kOsPasswordBlank,
                                false);
}

namespace password_manager_util {

const unsigned kMaxPasswordRetries = 3;

const unsigned kCredUiDefaultFlags =
    CREDUI_FLAGS_GENERIC_CREDENTIALS |
    CREDUI_FLAGS_EXCLUDE_CERTIFICATES |
    CREDUI_FLAGS_KEEP_USERNAME |
    CREDUI_FLAGS_ALWAYS_SHOW_UI |
    CREDUI_FLAGS_DO_NOT_PERSIST;

static int64 GetPasswordLastChanged(WCHAR* username) {
  LPUSER_INFO_1 user_info = NULL;
  DWORD age = 0;

  NET_API_STATUS ret = NetUserGetInfo(NULL, username, 1, (LPBYTE*) &user_info);

  if (ret == NERR_Success) {
    // Returns seconds since last password change.
    age = user_info->usri1_password_age;
    NetApiBufferFree(user_info);
  } else {
    return -1;
  }

  base::Time changed = base::Time::Now() - base::TimeDelta::FromSeconds(age);

  return changed.ToInternalValue();
}

static bool CheckBlankPassword(WCHAR* username) {
  PrefService* local_state = g_browser_process->local_state();
  int64 last_changed = GetPasswordLastChanged(username);
  bool need_recheck = true;
  bool blank_password = false;

  // If we cannot determine when the password was last changed
  // then assume the password is not blank
  if (last_changed == -1)
    return false;

  blank_password =
      local_state->GetBoolean(password_manager::prefs::kOsPasswordBlank);
  int64 pref_last_changed =
      local_state->GetInt64(password_manager::prefs::kOsPasswordLastChanged);
  if (pref_last_changed > 0 && last_changed <= pref_last_changed) {
    need_recheck = false;
  }

  if (need_recheck) {
    HANDLE handle = INVALID_HANDLE_VALUE;

    // Attempt to login using blank password.
    DWORD logon_result = LogonUser(username,
                                   L".",
                                   L"",
                                   LOGON32_LOGON_NETWORK,
                                   LOGON32_PROVIDER_DEFAULT,
                                   &handle);

    // Win XP and later return ERROR_ACCOUNT_RESTRICTION for blank password.
    if (logon_result)
      CloseHandle(handle);

    // In the case the password is blank, then LogonUser returns a failure,
    // handle is INVALID_HANDLE_VALUE, and GetLastError() is
    // ERROR_ACCOUNT_RESTRICTION.
    // http://msdn.microsoft.com/en-us/library/windows/desktop/ms681385
    blank_password = (logon_result ||
                      GetLastError() == ERROR_ACCOUNT_RESTRICTION);
  }

  // Account for clock skew between pulling the password age and
  // writing to the preferences by adding a small skew factor here.
  last_changed += base::Time::kMicrosecondsPerSecond;

  // Save the blank password status for later.
  local_state->SetBoolean(password_manager::prefs::kOsPasswordBlank,
                          blank_password);
  local_state->SetInt64(password_manager::prefs::kOsPasswordLastChanged,
                        last_changed);

  return blank_password;
}

OsPasswordStatus GetOsPasswordStatus() {
  DWORD username_length = CREDUI_MAX_USERNAME_LENGTH;
  WCHAR username[CREDUI_MAX_USERNAME_LENGTH+1] = {};
  OsPasswordStatus retVal = PASSWORD_STATUS_UNKNOWN;

  if (GetUserNameEx(NameUserPrincipal, username, &username_length)) {
    // If we are on a domain, it is almost certain that the password is not
    // blank, but we do not actively check any further than this to avoid any
    // failed login attempts hitting the domain controller.
    retVal = PASSWORD_STATUS_WIN_DOMAIN;
  } else {
    username_length = CREDUI_MAX_USERNAME_LENGTH;
    if (GetUserName(username, &username_length)) {
      retVal = CheckBlankPassword(username) ? PASSWORD_STATUS_BLANK :
          PASSWORD_STATUS_NONBLANK;
    }
  }

  return retVal;
}

bool AuthenticateUser(gfx::NativeWindow window) {
  bool retval = false;
  CREDUI_INFO cui = {};
  WCHAR username[CREDUI_MAX_USERNAME_LENGTH+1] = {};
  WCHAR displayname[CREDUI_MAX_USERNAME_LENGTH+1] = {};
  WCHAR password[CREDUI_MAX_PASSWORD_LENGTH+1] = {};
  DWORD username_length = CREDUI_MAX_USERNAME_LENGTH;
  base::string16 product_name = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
  base::string16 password_prompt =
      l10n_util::GetStringUTF16(IDS_PASSWORDS_PAGE_AUTHENTICATION_PROMPT);
  HANDLE handle = INVALID_HANDLE_VALUE;
  int tries = 0;
  bool use_displayname = false;
  bool use_principalname = false;
  DWORD logon_result = 0;

  // Disable password manager reauthentication before Windows 7.
  // This is because of an interaction between LogonUser() and the sandbox.
  // http://crbug.com/345916
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return true;

  // On a domain, we obtain the User Principal Name
  // for domain authentication.
  if (GetUserNameEx(NameUserPrincipal, username, &username_length)) {
    use_principalname = true;
  } else {
    username_length = CREDUI_MAX_USERNAME_LENGTH;
    // Otherwise, we're a workstation, use the plain local username.
    if (!GetUserName(username, &username_length)) {
      DLOG(ERROR) << "Unable to obtain username " << GetLastError();
      return false;
    } else {
      // As we are on a workstation, it's possible the user
      // has no password, so check here.
      if (CheckBlankPassword(username))
        return true;
    }
  }

  // Try and obtain a friendly display name.
  username_length = CREDUI_MAX_USERNAME_LENGTH;
  if (GetUserNameEx(NameDisplay, displayname, &username_length))
    use_displayname = true;

  cui.cbSize = sizeof(CREDUI_INFO);
  cui.hwndParent = NULL;
#if defined(USE_AURA)
  cui.hwndParent = window->GetHost()->GetAcceleratedWidget();
#else
  cui.hwndParent = window;
#endif

  cui.pszMessageText = password_prompt.c_str();
  cui.pszCaptionText = product_name.c_str();

  cui.hbmBanner = NULL;
  BOOL save_password = FALSE;
  DWORD credErr = NO_ERROR;

  do {
    tries++;

    // TODO(wfh) Make sure we support smart cards here.
    credErr = CredUIPromptForCredentials(
        &cui,
        product_name.c_str(),
        NULL,
        0,
        use_displayname ? displayname : username,
        CREDUI_MAX_USERNAME_LENGTH+1,
        password,
        CREDUI_MAX_PASSWORD_LENGTH+1,
        &save_password,
        kCredUiDefaultFlags |
        (tries > 1 ? CREDUI_FLAGS_INCORRECT_PASSWORD : 0));

    if (credErr == NO_ERROR) {
      logon_result = LogonUser(username,
                               use_principalname ? NULL : L".",
                               password,
                               LOGON32_LOGON_NETWORK,
                               LOGON32_PROVIDER_DEFAULT,
                               &handle);
      if (logon_result) {
        retval = true;
        CloseHandle(handle);
      } else {
        if (GetLastError() == ERROR_ACCOUNT_RESTRICTION &&
            wcslen(password) == 0) {
          // Password is blank, so permit.
          retval = true;
        } else {
          DLOG(WARNING) << "Unable to authenticate " << GetLastError();
        }
      }
      SecureZeroMemory(password, sizeof(password));
    }
  } while (credErr == NO_ERROR &&
           (retval == false && tries < kMaxPasswordRetries));
  return retval;
}

}  // namespace password_manager_util
