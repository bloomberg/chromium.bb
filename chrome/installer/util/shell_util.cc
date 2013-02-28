// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines functions that integrate Chrome in Windows shell. These
// functions can be used by Chrome as well as Chrome installer. All of the
// work is done by the local functions defined in anonymous namespace in
// this class.

#include "chrome/installer/util/shell_util.h"

#include <windows.h>
#include <shlobj.h>

#include <limits>
#include <string>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_comptr.h"
#include "base/win/shortcut.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "chrome/installer/util/util_constants.h"

#include "installer_util_strings.h"  // NOLINT

using base::win::RegKey;

namespace {

// An enum used to tell QuickIsChromeRegistered() which level of registration
// the caller wants to confirm.
enum RegistrationConfirmationLevel {
  // Only look for Chrome's ProgIds.
  // This is sufficient when we are trying to determine the suffix of the
  // currently running Chrome as shell integration registrations might not be
  // present.
  CONFIRM_PROGID_REGISTRATION = 0,
  // Confirm that Chrome is fully integrated with Windows (i.e. registered with
  // Defaut Programs). These registrations can be in HKCU as of Windows 8.
  // Note: Shell registration implies ProgId registration.
  CONFIRM_SHELL_REGISTRATION,
  // Same as CONFIRM_SHELL_REGISTRATION, but only look in HKLM (used when
  // uninstalling to know whether elevation is required to clean up the
  // registry).
  CONFIRM_SHELL_REGISTRATION_IN_HKLM,
};

const wchar_t kReinstallCommand[] = L"ReinstallCommand";

// Returns true if Chrome Metro is supported on this OS (Win 8 8370 or greater).
// TODO(gab): Change this to a simple check for Win 8 once old Win8 builds
// become irrelevant.
bool IsChromeMetroSupported() {
  OSVERSIONINFOEX min_version_info = {};
  min_version_info.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
  min_version_info.dwMajorVersion = 6;
  min_version_info.dwMinorVersion = 2;
  min_version_info.dwBuildNumber = 8370;
  min_version_info.wServicePackMajor = 0;
  min_version_info.wServicePackMinor = 0;

  DWORDLONG condition_mask = 0;
  VER_SET_CONDITION(condition_mask, VER_MAJORVERSION, VER_GREATER_EQUAL);
  VER_SET_CONDITION(condition_mask, VER_MINORVERSION, VER_GREATER_EQUAL);
  VER_SET_CONDITION(condition_mask, VER_BUILDNUMBER, VER_GREATER_EQUAL);
  VER_SET_CONDITION(condition_mask, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);
  VER_SET_CONDITION(condition_mask, VER_SERVICEPACKMINOR, VER_GREATER_EQUAL);

  DWORD type_mask = VER_MAJORVERSION | VER_MINORVERSION | VER_BUILDNUMBER |
      VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR;

  return VerifyVersionInfo(&min_version_info, type_mask, condition_mask) != 0;
}

// Returns the current (or installed) browser's ProgId (e.g.
// "ChromeHTML|suffix|").
// |suffix| can be the empty string.
string16 GetBrowserProgId(const string16& suffix) {
  string16 chrome_html(ShellUtil::kChromeHTMLProgId);
  chrome_html.append(suffix);

  // ProgIds cannot be longer than 39 characters.
  // Ref: http://msdn.microsoft.com/en-us/library/aa911706.aspx.
  // Make all new registrations comply with this requirement (existing
  // registrations must be preserved).
  string16 new_style_suffix;
  if (ShellUtil::GetUserSpecificRegistrySuffix(&new_style_suffix) &&
      suffix == new_style_suffix && chrome_html.length() > 39) {
    NOTREACHED();
    chrome_html.erase(39);
  }
  return chrome_html;
}

// This class is used to initialize and cache a base 32 encoding of the md5 hash
// of this user's sid preceded by a dot.
// This is guaranteed to be unique on the machine and 27 characters long
// (including the '.').
// This is then meant to be used as a suffix on all registrations that may
// conflict with another user-level Chrome install.
class UserSpecificRegistrySuffix {
 public:
  // All the initialization is done in the constructor to be able to build the
  // suffix in a thread-safe manner when used in conjunction with a
  // LazyInstance.
  UserSpecificRegistrySuffix();

  // Sets |suffix| to the pre-computed suffix cached in this object.
  // Returns true unless the initialization originally failed.
  bool GetSuffix(string16* suffix);

 private:
  string16 suffix_;

  DISALLOW_COPY_AND_ASSIGN(UserSpecificRegistrySuffix);
};  // class UserSpecificRegistrySuffix

UserSpecificRegistrySuffix::UserSpecificRegistrySuffix() {
  string16 user_sid;
  if (!base::win::GetUserSidString(&user_sid)) {
    NOTREACHED();
    return;
  }
  COMPILE_ASSERT(sizeof(base::MD5Digest) == 16, size_of_MD5_not_as_expected_);
  base::MD5Digest md5_digest;
  std::string user_sid_ascii(UTF16ToASCII(user_sid));
  base::MD5Sum(user_sid_ascii.c_str(), user_sid_ascii.length(), &md5_digest);
  const string16 base32_md5(
      ShellUtil::ByteArrayToBase32(md5_digest.a, arraysize(md5_digest.a)));
  // The value returned by the base32 algorithm above must never change and
  // must always be 26 characters long (i.e. if someone ever moves this to
  // base and implements the full base32 algorithm (i.e. with appended '='
  // signs in the output), they must provide a flag to allow this method to
  // still request the output with no appended '=' signs).
  DCHECK_EQ(base32_md5.length(), 26U);
  suffix_.reserve(base32_md5.length() + 1);
  suffix_.assign(1, L'.');
  suffix_.append(base32_md5);
}

bool UserSpecificRegistrySuffix::GetSuffix(string16* suffix) {
  if (suffix_.empty()) {
    NOTREACHED();
    return false;
  }
  suffix->assign(suffix_);
  return true;
}

// This class represents a single registry entry. The objective is to
// encapsulate all the registry entries required for registering Chrome at one
// place. This class can not be instantiated outside the class and the objects
// of this class type can be obtained only by calling a static method of this
// class.
class RegistryEntry {
 public:
  // A bit-field enum of places to look for this key in the Windows registry.
  enum LookForIn {
    LOOK_IN_HKCU = 1 << 0,
    LOOK_IN_HKLM = 1 << 1,
    LOOK_IN_HKCU_THEN_HKLM = LOOK_IN_HKCU | LOOK_IN_HKLM,
  };

  // Returns the Windows browser client registration key for Chrome.  For
  // example: "Software\Clients\StartMenuInternet\Chromium[.user]".  Strictly
  // speaking, we should use the name of the executable (e.g., "chrome.exe"),
  // but that ship has sailed.  The cost of switching now is re-prompting users
  // to make Chrome their default browser, which isn't polite.  |suffix| is the
  // user-specific registration suffix; see GetUserSpecificDefaultBrowserSuffix
  // in shell_util.h for details.
  static string16 GetBrowserClientKey(BrowserDistribution* dist,
                                      const string16& suffix) {
    DCHECK(suffix.empty() || suffix[0] == L'.');
    return string16(ShellUtil::kRegStartMenuInternet)
        .append(1, L'\\')
        .append(dist->GetBaseAppName())
        .append(suffix);
  }

  // Returns the Windows Default Programs capabilities key for Chrome.  For
  // example:
  // "Software\Clients\StartMenuInternet\Chromium[.user]\Capabilities".
  static string16 GetCapabilitiesKey(BrowserDistribution* dist,
                                     const string16& suffix) {
    return GetBrowserClientKey(dist, suffix).append(L"\\Capabilities");
  }

  // This method returns a list of all the registry entries that
  // are needed to register this installation's ProgId and AppId.
  // These entries need to be registered in HKLM prior to Win8.
  static void GetProgIdEntries(BrowserDistribution* dist,
                               const string16& chrome_exe,
                               const string16& suffix,
                               ScopedVector<RegistryEntry>* entries) {
    string16 icon_path(
        ShellUtil::FormatIconLocation(chrome_exe, dist->GetIconIndex()));
    string16 open_cmd(ShellUtil::GetChromeShellOpenCmd(chrome_exe));
    string16 delegate_command(ShellUtil::GetChromeDelegateCommand(chrome_exe));
    // For user-level installs: entries for the app id and DelegateExecute verb
    // handler will be in HKCU; thus we do not need a suffix on those entries.
    string16 app_id(
        ShellUtil::GetBrowserModelId(
            dist, InstallUtil::IsPerUserInstall(chrome_exe.c_str())));
    string16 delegate_guid;
    bool set_delegate_execute =
        IsChromeMetroSupported() &&
        dist->GetCommandExecuteImplClsid(&delegate_guid);

    // DelegateExecute ProgId. Needed for Chrome Metro in Windows 8.
    if (set_delegate_execute) {
      string16 model_id_shell(ShellUtil::kRegClasses);
      model_id_shell.push_back(base::FilePath::kSeparators[0]);
      model_id_shell.append(app_id);
      model_id_shell.append(ShellUtil::kRegExePath);
      model_id_shell.append(ShellUtil::kRegShellPath);

      // <root hkey>\Software\Classes\<app_id>\.exe\shell @=open
      entries->push_back(new RegistryEntry(model_id_shell,
                                           ShellUtil::kRegVerbOpen));

      // Each of Chrome's shortcuts has an appid; which, as of Windows 8, is
      // registered to handle some verbs. This registration has the side-effect
      // that these verbs now show up in the shortcut's context menu. We
      // mitigate this side-effect by making the context menu entries
      // user readable/localized strings. See relevant MSDN article:
      // http://msdn.microsoft.com/en-US/library/windows/desktop/cc144171.aspx
      const struct {
        const wchar_t* verb;
        int name_id;
      } verbs[] = {
          { ShellUtil::kRegVerbOpen, -1 },
          { ShellUtil::kRegVerbOpenNewWindow, IDS_SHORTCUT_NEW_WINDOW_BASE },
      };
      for (size_t i = 0; i < arraysize(verbs); ++i) {
        string16 sub_path(model_id_shell);
        sub_path.push_back(base::FilePath::kSeparators[0]);
        sub_path.append(verbs[i].verb);

        // <root hkey>\Software\Classes\<app_id>\.exe\shell\<verb>
        if (verbs[i].name_id != -1) {
          // TODO(grt): http://crbug.com/75152 Write a reference to a localized
          // resource.
          string16 verb_name(installer::GetLocalizedString(verbs[i].name_id));
          entries->push_back(new RegistryEntry(sub_path, verb_name.c_str()));
        }
        entries->push_back(new RegistryEntry(
            sub_path, L"CommandId", L"Browser.Launch"));

        sub_path.push_back(base::FilePath::kSeparators[0]);
        sub_path.append(ShellUtil::kRegCommand);

        // <root hkey>\Software\Classes\<app_id>\.exe\shell\<verb>\command
        entries->push_back(new RegistryEntry(sub_path, delegate_command));
        entries->push_back(new RegistryEntry(
            sub_path, ShellUtil::kRegDelegateExecute, delegate_guid));
      }
    }

    // File association ProgId
    string16 chrome_html_prog_id(ShellUtil::kRegClasses);
    chrome_html_prog_id.push_back(base::FilePath::kSeparators[0]);
    chrome_html_prog_id.append(GetBrowserProgId(suffix));
    entries->push_back(new RegistryEntry(
        chrome_html_prog_id, ShellUtil::kChromeHTMLProgIdDesc));
    entries->push_back(new RegistryEntry(
        chrome_html_prog_id, ShellUtil::kRegUrlProtocol, L""));
    entries->push_back(new RegistryEntry(
        chrome_html_prog_id + ShellUtil::kRegDefaultIcon, icon_path));
    entries->push_back(new RegistryEntry(
        chrome_html_prog_id + ShellUtil::kRegShellOpen, open_cmd));
    if (set_delegate_execute) {
      entries->push_back(new RegistryEntry(
          chrome_html_prog_id + ShellUtil::kRegShellOpen,
          ShellUtil::kRegDelegateExecute, delegate_guid));
    }

    // The following entries are required as of Windows 8, but do not
    // depend on the DelegateExecute verb handler being set.
    if (base::win::GetVersion() >= base::win::VERSION_WIN8) {
      entries->push_back(new RegistryEntry(
          chrome_html_prog_id, ShellUtil::kRegAppUserModelId, app_id));

      // Add \Software\Classes\ChromeHTML\Application entries
      string16 chrome_application(chrome_html_prog_id +
                                  ShellUtil::kRegApplication);
      entries->push_back(new RegistryEntry(
          chrome_application, ShellUtil::kRegAppUserModelId, app_id));
      entries->push_back(new RegistryEntry(
          chrome_application, ShellUtil::kRegApplicationIcon, icon_path));
      // TODO(grt): http://crbug.com/75152 Write a reference to a localized
      // resource for name, description, and company.
      entries->push_back(new RegistryEntry(
          chrome_application, ShellUtil::kRegApplicationName,
          dist->GetAppShortCutName()));
      entries->push_back(new RegistryEntry(
          chrome_application, ShellUtil::kRegApplicationDescription,
          dist->GetAppDescription()));
      entries->push_back(new RegistryEntry(
          chrome_application, ShellUtil::kRegApplicationCompany,
          dist->GetPublisherName()));
    }
  }

  // This method returns a list of the registry entries needed to declare a
  // capability of handling a protocol on Windows.
  static void GetProtocolCapabilityEntries(
      BrowserDistribution* dist,
      const string16& suffix,
      const string16& protocol,
      ScopedVector<RegistryEntry>* entries) {
    entries->push_back(new RegistryEntry(
        GetCapabilitiesKey(dist, suffix).append(L"\\URLAssociations"),
        protocol, GetBrowserProgId(suffix)));
  }

  // This method returns a list of the registry entries required to register
  // this installation in "RegisteredApplications" on Windows (to appear in
  // Default Programs, StartMenuInternet, etc.).
  // These entries need to be registered in HKLM prior to Win8.
  // If |suffix| is not empty, these entries are guaranteed to be unique on this
  // machine.
  static void GetShellIntegrationEntries(BrowserDistribution* dist,
                                         const string16& chrome_exe,
                                         const string16& suffix,
                                         ScopedVector<RegistryEntry>* entries) {
    const string16 icon_path(
        ShellUtil::FormatIconLocation(chrome_exe, dist->GetIconIndex()));
    const string16 quoted_exe_path(L"\"" + chrome_exe + L"\"");

    // Register for the Start Menu "Internet" link (pre-Win7).
    const string16 start_menu_entry(GetBrowserClientKey(dist, suffix));
    // Register Chrome's display name.
    // TODO(grt): http://crbug.com/75152 Also set LocalizedString; see
    // http://msdn.microsoft.com/en-us/library/windows/desktop/cc144109(v=VS.85).aspx#registering_the_display_name
    entries->push_back(new RegistryEntry(
        start_menu_entry, dist->GetAppShortCutName()));
    // Register the "open" verb for launching Chrome via the "Internet" link.
    entries->push_back(new RegistryEntry(
        start_menu_entry + ShellUtil::kRegShellOpen, quoted_exe_path));
    // Register Chrome's icon for the Start Menu "Internet" link.
    entries->push_back(new RegistryEntry(
        start_menu_entry + ShellUtil::kRegDefaultIcon, icon_path));

    // Register installation information.
    string16 install_info(start_menu_entry + L"\\InstallInfo");
    // Note: not using CommandLine since it has ambiguous rules for quoting
    // strings.
    entries->push_back(new RegistryEntry(install_info, kReinstallCommand,
        quoted_exe_path + L" --" + ASCIIToWide(switches::kMakeDefaultBrowser)));
    entries->push_back(new RegistryEntry(install_info, L"HideIconsCommand",
        quoted_exe_path + L" --" + ASCIIToWide(switches::kHideIcons)));
    entries->push_back(new RegistryEntry(install_info, L"ShowIconsCommand",
        quoted_exe_path + L" --" + ASCIIToWide(switches::kShowIcons)));
    entries->push_back(new RegistryEntry(install_info, L"IconsVisible", 1));

    // Register with Default Programs.
    const string16 reg_app_name(dist->GetBaseAppName().append(suffix));
    // Tell Windows where to find Chrome's Default Programs info.
    const string16 capabilities(GetCapabilitiesKey(dist, suffix));
    entries->push_back(new RegistryEntry(ShellUtil::kRegRegisteredApplications,
        reg_app_name, capabilities));
    // Write out Chrome's Default Programs info.
    // TODO(grt): http://crbug.com/75152 Write a reference to a localized
    // resource rather than this.
    entries->push_back(new RegistryEntry(
        capabilities, ShellUtil::kRegApplicationDescription,
        dist->GetLongAppDescription()));
    entries->push_back(new RegistryEntry(
        capabilities, ShellUtil::kRegApplicationIcon, icon_path));
    entries->push_back(new RegistryEntry(
        capabilities, ShellUtil::kRegApplicationName,
        dist->GetAppShortCutName()));

    entries->push_back(new RegistryEntry(capabilities + L"\\Startmenu",
        L"StartMenuInternet", reg_app_name));

    const string16 html_prog_id(GetBrowserProgId(suffix));
    for (int i = 0; ShellUtil::kFileAssociations[i] != NULL; i++) {
      entries->push_back(new RegistryEntry(
          capabilities + L"\\FileAssociations",
          ShellUtil::kFileAssociations[i], html_prog_id));
    }
    for (int i = 0; ShellUtil::kPotentialProtocolAssociations[i] != NULL;
        i++) {
      entries->push_back(new RegistryEntry(
          capabilities + L"\\URLAssociations",
          ShellUtil::kPotentialProtocolAssociations[i], html_prog_id));
    }
  }

  // This method returns a list of the registry entries required for this
  // installation to be registered in the Windows shell.
  // In particular:
  //  - App Paths
  //    http://msdn.microsoft.com/en-us/library/windows/desktop/ee872121
  //  - File Associations
  //    http://msdn.microsoft.com/en-us/library/bb166549
  // These entries need to be registered in HKLM prior to Win8.
  static void GetAppRegistrationEntries(const string16& chrome_exe,
                                        const string16& suffix,
                                        ScopedVector<RegistryEntry>* entries) {
    const base::FilePath chrome_path(chrome_exe);
    string16 app_path_key(ShellUtil::kAppPathsRegistryKey);
    app_path_key.push_back(base::FilePath::kSeparators[0]);
    app_path_key.append(chrome_path.BaseName().value());
    entries->push_back(new RegistryEntry(app_path_key, chrome_exe));
    entries->push_back(new RegistryEntry(app_path_key,
        ShellUtil::kAppPathsRegistryPathName, chrome_path.DirName().value()));

    const string16 html_prog_id(GetBrowserProgId(suffix));
    for (int i = 0; ShellUtil::kFileAssociations[i] != NULL; i++) {
      string16 key(ShellUtil::kRegClasses);
      key.push_back(base::FilePath::kSeparators[0]);
      key.append(ShellUtil::kFileAssociations[i]);
      key.push_back(base::FilePath::kSeparators[0]);
      key.append(ShellUtil::kRegOpenWithProgids);
      entries->push_back(new RegistryEntry(key, html_prog_id, string16()));
    }
  }

  // This method returns a list of all the user level registry entries that
  // are needed to make Chromium the default handler for a protocol on XP.
  static void GetXPStyleUserProtocolEntries(
      const string16& protocol,
      const string16& chrome_icon,
      const string16& chrome_open,
      ScopedVector<RegistryEntry>* entries) {
    // Protocols associations.
    string16 url_key(ShellUtil::kRegClasses);
    url_key.push_back(base::FilePath::kSeparators[0]);
    url_key.append(protocol);

    // This registry value tells Windows that this 'class' is a URL scheme
    // so IE, explorer and other apps will route it to our handler.
    // <root hkey>\Software\Classes\<protocol>\URL Protocol
    entries->push_back(new RegistryEntry(url_key,
        ShellUtil::kRegUrlProtocol, L""));

    // <root hkey>\Software\Classes\<protocol>\DefaultIcon
    string16 icon_key = url_key + ShellUtil::kRegDefaultIcon;
    entries->push_back(new RegistryEntry(icon_key, chrome_icon));

    // <root hkey>\Software\Classes\<protocol>\shell\open\command
    string16 shell_key = url_key + ShellUtil::kRegShellOpen;
    entries->push_back(new RegistryEntry(shell_key, chrome_open));

    // <root hkey>\Software\Classes\<protocol>\shell\open\ddeexec
    string16 dde_key = url_key + L"\\shell\\open\\ddeexec";
    entries->push_back(new RegistryEntry(dde_key, L""));

    // <root hkey>\Software\Classes\<protocol>\shell\@
    string16 protocol_shell_key = url_key + ShellUtil::kRegShellPath;
    entries->push_back(new RegistryEntry(protocol_shell_key, L"open"));
  }

  // This method returns a list of all the user level registry entries that
  // are needed to make Chromium default browser on XP.
  // Some of these entries are irrelevant in recent versions of Windows, but
  // we register them anyways as some legacy apps are hardcoded to lookup those
  // values.
  static void GetXPStyleDefaultBrowserUserEntries(
      BrowserDistribution* dist,
      const string16& chrome_exe,
      const string16& suffix,
      ScopedVector<RegistryEntry>* entries) {
    // File extension associations.
    string16 html_prog_id(GetBrowserProgId(suffix));
    for (int i = 0; ShellUtil::kFileAssociations[i] != NULL; i++) {
      string16 ext_key(ShellUtil::kRegClasses);
      ext_key.push_back(base::FilePath::kSeparators[0]);
      ext_key.append(ShellUtil::kFileAssociations[i]);
      entries->push_back(new RegistryEntry(ext_key, html_prog_id));
    }

    // Protocols associations.
    string16 chrome_open = ShellUtil::GetChromeShellOpenCmd(chrome_exe);
    string16 chrome_icon =
        ShellUtil::FormatIconLocation(chrome_exe, dist->GetIconIndex());
    for (int i = 0; ShellUtil::kBrowserProtocolAssociations[i] != NULL; i++) {
      GetXPStyleUserProtocolEntries(ShellUtil::kBrowserProtocolAssociations[i],
                                    chrome_icon, chrome_open, entries);
    }

    // start->Internet shortcut.
    string16 start_menu(ShellUtil::kRegStartMenuInternet);
    string16 app_name = dist->GetBaseAppName() + suffix;
    entries->push_back(new RegistryEntry(start_menu, app_name));
  }

  // Generate work_item tasks required to create current registry entry and
  // add them to the given work item list.
  void AddToWorkItemList(HKEY root, WorkItemList *items) const {
    items->AddCreateRegKeyWorkItem(root, _key_path);
    if (_is_string) {
      items->AddSetRegValueWorkItem(root, _key_path, _name, _value, true);
    } else {
      items->AddSetRegValueWorkItem(root, _key_path, _name, _int_value, true);
    }
  }

  // Checks if the current registry entry exists in HKCU\|_key_path|\|_name|
  // and value is |_value|. If the key does NOT exist in HKCU, checks for
  // the correct name and value in HKLM.
  // |look_for_in| specifies roots (HKCU and/or HKLM) in which to look for the
  // key, unspecified roots are not looked into (i.e. the the key is assumed not
  // to exist in them).
  // |look_for_in| must at least specify one root to look into.
  // If |look_for_in| is LOOK_IN_HKCU_THEN_HKLM, this method mimics Windows'
  // behavior when searching in HKCR (HKCU takes precedence over HKLM). For
  // registrations outside of HKCR on versions of Windows prior to Win8,
  // Chrome's values go in HKLM. This function will make unnecessary (but
  // harmless) queries into HKCU in that case.
  bool ExistsInRegistry(uint32 look_for_in) const {
    DCHECK(look_for_in);

    RegistryStatus status = DOES_NOT_EXIST;
    if (look_for_in & LOOK_IN_HKCU)
      status = StatusInRegistryUnderRoot(HKEY_CURRENT_USER);
    if (status == DOES_NOT_EXIST && (look_for_in & LOOK_IN_HKLM))
      status = StatusInRegistryUnderRoot(HKEY_LOCAL_MACHINE);
    return status == SAME_VALUE;
  }

 private:
  // States this RegistryKey can be in compared to the registry.
  enum RegistryStatus {
    // |_name| does not exist in the registry
    DOES_NOT_EXIST,
    // |_name| exists, but its value != |_value|
    DIFFERENT_VALUE,
    // |_name| exists and its value is |_value|
    SAME_VALUE,
  };

  // Create a object that represent default value of a key
  RegistryEntry(const string16& key_path, const string16& value)
      : _key_path(key_path), _name(),
        _is_string(true), _value(value), _int_value(0) {
  }

  // Create a object that represent a key of type REG_SZ
  RegistryEntry(const string16& key_path, const string16& name,
                const string16& value)
      : _key_path(key_path), _name(name),
        _is_string(true), _value(value), _int_value(0) {
  }

  // Create a object that represent a key of integer type
  RegistryEntry(const string16& key_path, const string16& name,
                DWORD value)
      : _key_path(key_path), _name(name),
        _is_string(false), _value(), _int_value(value) {
  }

  string16 _key_path;  // key path for the registry entry
  string16 _name;      // name of the registry entry
  bool _is_string;     // true if current registry entry is of type REG_SZ
  string16 _value;     // string value (useful if _is_string = true)
  DWORD _int_value;    // integer value (useful if _is_string = false)

  // Helper function for ExistsInRegistry().
  // Returns the RegistryStatus of the current registry entry in
  // |root|\|_key_path|\|_name|.
  RegistryStatus StatusInRegistryUnderRoot(HKEY root) const {
    RegKey key(root, _key_path.c_str(), KEY_QUERY_VALUE);
    bool found = false;
    bool correct_value = false;
    if (_is_string) {
      string16 read_value;
      found = key.ReadValue(_name.c_str(), &read_value) == ERROR_SUCCESS;
      correct_value = read_value.size() == _value.size() &&
          std::equal(_value.begin(), _value.end(), read_value.begin(),
                     base::CaseInsensitiveCompare<wchar_t>());
    } else {
      DWORD read_value;
      found = key.ReadValueDW(_name.c_str(), &read_value) == ERROR_SUCCESS;
      correct_value = read_value == _int_value;
    }
    return found ?
        (correct_value ? SAME_VALUE : DIFFERENT_VALUE) : DOES_NOT_EXIST;
  }

  DISALLOW_COPY_AND_ASSIGN(RegistryEntry);
};  // class RegistryEntry


// This method converts all the RegistryEntries from the given list to
// Set/CreateRegWorkItems and runs them using WorkItemList.
bool AddRegistryEntries(HKEY root, const ScopedVector<RegistryEntry>& entries) {
  scoped_ptr<WorkItemList> items(WorkItem::CreateWorkItemList());

  for (ScopedVector<RegistryEntry>::const_iterator itr = entries.begin();
       itr != entries.end(); ++itr)
    (*itr)->AddToWorkItemList(root, items.get());

  // Apply all the registry changes and if there is a problem, rollback
  if (!items->Do()) {
    items->Rollback();
    return false;
  }
  return true;
}

// Checks that all |entries| are present on this computer.
// |look_for_in| is passed to RegistryEntry::ExistsInRegistry(). Documentation
// for it can be found there.
bool AreEntriesRegistered(const ScopedVector<RegistryEntry>& entries,
                          uint32 look_for_in) {
  bool registered = true;
  for (ScopedVector<RegistryEntry>::const_iterator itr = entries.begin();
       registered && itr != entries.end(); ++itr) {
    // We do not need registered = registered && ... since the loop condition
    // is set to exit early.
    registered = (*itr)->ExistsInRegistry(look_for_in);
  }
  return registered;
}

// Checks that all required registry entries for Chrome are already present
// on this computer.
// Note: between r133333 and r154145 we were registering parts of Chrome in HKCU
// and parts in HKLM for user-level installs; we now always register everything
// under a single registry root. Not doing so caused http://crbug.com/144910 for
// users who first-installed Chrome in that revision range (those users are
// still impacted by http://crbug.com/144910). This method will keep returning
// true for affected users (i.e. who have all the registrations, but over both
// registry roots).
bool IsChromeRegistered(BrowserDistribution* dist,
                        const string16& chrome_exe,
                        const string16& suffix) {
  ScopedVector<RegistryEntry> entries;
  RegistryEntry::GetProgIdEntries(dist, chrome_exe, suffix, &entries);
  RegistryEntry::GetShellIntegrationEntries(dist, chrome_exe, suffix, &entries);
  RegistryEntry::GetAppRegistrationEntries(chrome_exe, suffix, &entries);
  return AreEntriesRegistered(entries, RegistryEntry::LOOK_IN_HKCU_THEN_HKLM);
}

// This method checks if Chrome is already registered on the local machine
// for the requested protocol. It just checks the one value required for this.
bool IsChromeRegisteredForProtocol(BrowserDistribution* dist,
                                   const string16& suffix,
                                   const string16& protocol) {
  ScopedVector<RegistryEntry> entries;
  RegistryEntry::GetProtocolCapabilityEntries(dist, suffix, protocol, &entries);
  return AreEntriesRegistered(entries, RegistryEntry::LOOK_IN_HKCU_THEN_HKLM);
}

// This method registers Chrome on Vista by launching an elevated setup.exe.
// That will show the user the standard Vista elevation prompt. If the user
// accepts it the new process will make the necessary changes and return SUCCESS
// that we capture and return.
// If protocol is non-empty we will also register Chrome as being capable of
// handling the protocol.
bool ElevateAndRegisterChrome(BrowserDistribution* dist,
                              const string16& chrome_exe,
                              const string16& suffix,
                              const string16& protocol) {
  // Only user-level installs prior to Windows 8 should need to elevate to
  // register.
  DCHECK(InstallUtil::IsPerUserInstall(chrome_exe.c_str()));
  DCHECK_LT(base::win::GetVersion(), base::win::VERSION_WIN8);
  base::FilePath exe_path =
      base::FilePath::FromWStringHack(chrome_exe).DirName()
          .Append(installer::kSetupExe);
  if (!file_util::PathExists(exe_path)) {
    HKEY reg_root = InstallUtil::IsPerUserInstall(chrome_exe.c_str()) ?
        HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
    RegKey key(reg_root, dist->GetUninstallRegPath().c_str(), KEY_READ);
    string16 uninstall_string;
    key.ReadValue(installer::kUninstallStringField, &uninstall_string);
    CommandLine command_line = CommandLine::FromString(uninstall_string);
    exe_path = command_line.GetProgram();
  }

  if (file_util::PathExists(exe_path)) {
    CommandLine cmd(exe_path);
    cmd.AppendSwitchNative(installer::switches::kRegisterChromeBrowser,
                           chrome_exe);
    if (!suffix.empty()) {
      cmd.AppendSwitchNative(
          installer::switches::kRegisterChromeBrowserSuffix, suffix);
    }

    CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
    if (browser_command_line.HasSwitch(switches::kChromeFrame)) {
      cmd.AppendSwitch(installer::switches::kChromeFrame);
    }

    if (!protocol.empty()) {
      cmd.AppendSwitchNative(
        installer::switches::kRegisterURLProtocol, protocol);
    }

    DWORD ret_val = 0;
    InstallUtil::ExecuteExeAsAdmin(cmd, &ret_val);
    if (ret_val == 0)
      return true;
  }
  return false;
}

// Launches the Windows 7 and Windows 8 dialog for picking the application to
// handle the given protocol. Most importantly, this is used to set the default
// handler for http (and, implicitly with it, https). In that case it is also
// known as the 'how do you want to open webpages' dialog.
// It is required that Chrome be already *registered* for the given protocol.
bool LaunchSelectDefaultProtocolHandlerDialog(const wchar_t* protocol) {
  DCHECK(protocol);
  OPENASINFO open_as_info = {};
  open_as_info.pcszFile = protocol;
  open_as_info.oaifInFlags =
      OAIF_URL_PROTOCOL | OAIF_FORCE_REGISTRATION | OAIF_REGISTER_EXT;
  HRESULT hr = SHOpenWithDialog(NULL, &open_as_info);
  DLOG_IF(WARNING, FAILED(hr)) << "Failed to set as default " << protocol
      << " handler; hr=0x" << std::hex << hr;
  if (FAILED(hr))
    return false;
  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
  return true;
}

// Launches the Windows 7 and Windows 8 application association dialog, which
// is the only documented way to make a browser the default browser on
// Windows 8.
bool LaunchApplicationAssociationDialog(const string16& app_id) {
  base::win::ScopedComPtr<IApplicationAssociationRegistrationUI> aarui;
  HRESULT hr = aarui.CreateInstance(CLSID_ApplicationAssociationRegistrationUI);
  if (FAILED(hr))
    return false;
  hr = aarui->LaunchAdvancedAssociationUI(app_id.c_str());
  return SUCCEEDED(hr);
}

// Returns true if the current install's |chrome_exe| has been registered with
// |suffix|.
// |confirmation_level| is the level of verification desired as described in
// the RegistrationConfirmationLevel enum above.
// |suffix| can be the empty string (this is used to support old installs
// where we used to not suffix user-level installs if they were the first to
// request the non-suffixed registry entries on the machine).
// NOTE: This a quick check that only validates that a single registry entry
// points to |chrome_exe|. This should only be used at run-time to determine
// how Chrome is registered, not to know whether the registration is complete
// at install-time (IsChromeRegistered() can be used for that).
bool QuickIsChromeRegistered(BrowserDistribution* dist,
                             const string16& chrome_exe,
                             const string16& suffix,
                             RegistrationConfirmationLevel confirmation_level) {
  // Get the appropriate key to look for based on the level desired.
  string16 reg_key;
  switch (confirmation_level) {
    case CONFIRM_PROGID_REGISTRATION:
      // Software\Classes\ChromeHTML|suffix|
      reg_key = ShellUtil::kRegClasses;
      reg_key.push_back(base::FilePath::kSeparators[0]);
      reg_key.append(ShellUtil::kChromeHTMLProgId);
      reg_key.append(suffix);
      break;
    case CONFIRM_SHELL_REGISTRATION:
    case CONFIRM_SHELL_REGISTRATION_IN_HKLM:
      // Software\Clients\StartMenuInternet\Google Chrome|suffix|
      reg_key = RegistryEntry::GetBrowserClientKey(dist, suffix);
      break;
    default:
      NOTREACHED();
      break;
  }
  reg_key.append(ShellUtil::kRegShellOpen);

  // ProgId registrations are allowed to reside in HKCU for user-level installs
  // (and values there have priority over values in HKLM). The same is true for
  // shell integration entries as of Windows 8.
  if (confirmation_level == CONFIRM_PROGID_REGISTRATION ||
      (confirmation_level == CONFIRM_SHELL_REGISTRATION &&
       base::win::GetVersion() >= base::win::VERSION_WIN8)) {
    const RegKey key_hkcu(HKEY_CURRENT_USER, reg_key.c_str(), KEY_QUERY_VALUE);
    string16 hkcu_value;
    // If |reg_key| is present in HKCU, assert that it points to |chrome_exe|.
    // Otherwise, fall back on an HKLM lookup below.
    if (key_hkcu.ReadValue(L"", &hkcu_value) == ERROR_SUCCESS) {
      return InstallUtil::ProgramCompare(
          base::FilePath(chrome_exe)).Evaluate(hkcu_value);
    }
  }

  // Assert that |reg_key| points to |chrome_exe| in HKLM.
  const RegKey key_hklm(HKEY_LOCAL_MACHINE, reg_key.c_str(), KEY_QUERY_VALUE);
  string16 hklm_value;
  if (key_hklm.ReadValue(L"", &hklm_value) == ERROR_SUCCESS) {
    return InstallUtil::ProgramCompare(
        base::FilePath(chrome_exe)).Evaluate(hklm_value);
  }
  return false;
}

// Sets |suffix| to a 27 character string that is specific to this user on this
// machine (on user-level installs only).
// To support old-style user-level installs however, |suffix| is cleared if the
// user currently owns the non-suffixed HKLM registrations.
// |suffix| can also be set to the user's username if the current install is
// suffixed as per the old-style registrations.
// |suffix| is cleared on system-level installs.
// |suffix| should then be appended to all Chrome properties that may conflict
// with other Chrome user-level installs.
// Returns true unless one of the underlying calls fails.
bool GetInstallationSpecificSuffix(BrowserDistribution* dist,
                                   const string16& chrome_exe,
                                   string16* suffix) {
  if (!InstallUtil::IsPerUserInstall(chrome_exe.c_str()) ||
      QuickIsChromeRegistered(dist, chrome_exe, string16(),
                              CONFIRM_SHELL_REGISTRATION)) {
    // No suffix on system-level installs and user-level installs already
    // registered with no suffix.
    suffix->clear();
    return true;
  }

  // Get the old suffix for the check below.
  if (!ShellUtil::GetOldUserSpecificRegistrySuffix(suffix)) {
    NOTREACHED();
    return false;
  }
  if (QuickIsChromeRegistered(dist, chrome_exe, *suffix,
                              CONFIRM_SHELL_REGISTRATION)) {
    // Username suffix for installs that are suffixed as per the old-style.
    return true;
  }

  return ShellUtil::GetUserSpecificRegistrySuffix(suffix);
}

// Returns the root registry key (HKLM or HKCU) under which registrations must
// be placed for this install. As of Windows 8 everything can go in HKCU for
// per-user installs.
HKEY DetermineRegistrationRoot(bool is_per_user) {
  return is_per_user && base::win::GetVersion() >= base::win::VERSION_WIN8 ?
      HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
}

// Associates Chrome with supported protocols and file associations. This should
// not be required on Vista+ but since some applications still read
// Software\Classes\http key directly, we have to do this on Vista+ as well.
bool RegisterChromeAsDefaultXPStyle(BrowserDistribution* dist,
                                    int shell_change,
                                    const string16& chrome_exe) {
  bool ret = true;
  ScopedVector<RegistryEntry> entries;
  RegistryEntry::GetXPStyleDefaultBrowserUserEntries(
      dist, chrome_exe,
      ShellUtil::GetCurrentInstallationSuffix(dist, chrome_exe), &entries);

  // Change the default browser for current user.
  if ((shell_change & ShellUtil::CURRENT_USER) &&
      !AddRegistryEntries(HKEY_CURRENT_USER, entries)) {
    ret = false;
    LOG(ERROR) << "Could not make Chrome default browser (XP/current user).";
  }

  // Chrome as default browser at system level.
  if ((shell_change & ShellUtil::SYSTEM_LEVEL) &&
      !AddRegistryEntries(HKEY_LOCAL_MACHINE, entries)) {
    ret = false;
    LOG(ERROR) << "Could not make Chrome default browser (XP/system level).";
  }

  return ret;
}

// Associates Chrome with |protocol| in the registry. This should not be
// required on Vista+ but since some applications still read these registry
// keys directly, we have to do this on Vista+ as well.
// See http://msdn.microsoft.com/library/aa767914.aspx for more details.
bool RegisterChromeAsDefaultProtocolClientXPStyle(BrowserDistribution* dist,
                                                  const string16& chrome_exe,
                                                  const string16& protocol) {
  ScopedVector<RegistryEntry> entries;
  const string16 chrome_open(ShellUtil::GetChromeShellOpenCmd(chrome_exe));
  const string16 chrome_icon(
      ShellUtil::FormatIconLocation(chrome_exe, dist->GetIconIndex()));
  RegistryEntry::GetXPStyleUserProtocolEntries(protocol, chrome_icon,
                                               chrome_open, &entries);
  // Change the default protocol handler for current user.
  if (!AddRegistryEntries(HKEY_CURRENT_USER, entries)) {
    LOG(ERROR) << "Could not make Chrome default protocol client (XP).";
    return false;
  }

  return true;
}

// Returns |properties.shortcut_name| if the property is set, otherwise it
// returns dist->GetAppShortcutName(). In any case, it makes sure the
// return value is suffixed with ".lnk".
string16 ExtractShortcutNameFromProperties(
    BrowserDistribution* dist,
    const ShellUtil::ShortcutProperties& properties) {
  DCHECK(dist);
  string16 shortcut_name;
  if (properties.has_shortcut_name())
    shortcut_name = properties.shortcut_name;
  else
    shortcut_name = dist->GetAppShortCutName();

  if (!EndsWith(shortcut_name, installer::kLnkExt, false))
    shortcut_name.append(installer::kLnkExt);

  return shortcut_name;
}

// Converts ShellUtil::ShortcutOperation to the best-matching value in
// base::win::ShortcutOperation.
base::win::ShortcutOperation TranslateShortcutOperation(
    ShellUtil::ShortcutOperation operation) {
  switch (operation) {
    case ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS:  // Falls through.
    case ShellUtil::SHELL_SHORTCUT_CREATE_IF_NO_SYSTEM_LEVEL:
      return base::win::SHORTCUT_CREATE_ALWAYS;

    case ShellUtil::SHELL_SHORTCUT_UPDATE_EXISTING:
      return base::win::SHORTCUT_UPDATE_EXISTING;

    case ShellUtil::SHELL_SHORTCUT_REPLACE_EXISTING:
      return base::win::SHORTCUT_REPLACE_EXISTING;

    default:
      NOTREACHED();
      return base::win::SHORTCUT_REPLACE_EXISTING;
  }
}

// Returns a base::win::ShortcutProperties struct containing the properties
// to set on the shortcut based on the provided ShellUtil::ShortcutProperties.
base::win::ShortcutProperties TranslateShortcutProperties(
    const ShellUtil::ShortcutProperties& properties) {
  base::win::ShortcutProperties shortcut_properties;

  if (properties.has_target()) {
    shortcut_properties.set_target(properties.target);
    DCHECK(!properties.target.DirName().empty());
    shortcut_properties.set_working_dir(properties.target.DirName());
  }

  if (properties.has_arguments())
    shortcut_properties.set_arguments(properties.arguments);

  if (properties.has_description())
    shortcut_properties.set_description(properties.description);

  if (properties.has_icon())
    shortcut_properties.set_icon(properties.icon, properties.icon_index);

  if (properties.has_app_id())
    shortcut_properties.set_app_id(properties.app_id);

  if (properties.has_dual_mode())
    shortcut_properties.set_dual_mode(properties.dual_mode);

  return shortcut_properties;
}

// Cleans up an old verb (run) we used to register in
// <root>\Software\Classes\Chrome<.suffix>\.exe\shell\run on Windows 8.
void RemoveRunVerbOnWindows8(
    BrowserDistribution* dist,
    const string16& chrome_exe) {
  if (IsChromeMetroSupported()) {
    bool is_per_user_install =InstallUtil::IsPerUserInstall(chrome_exe.c_str());
    HKEY root_key = DetermineRegistrationRoot(is_per_user_install);
    // There's no need to rollback, so forgo the usual work item lists and just
    // remove the key from the registry.
    string16 run_verb_key(ShellUtil::kRegClasses);
    run_verb_key.push_back(base::FilePath::kSeparators[0]);
    run_verb_key.append(ShellUtil::GetBrowserModelId(
        dist, is_per_user_install));
    run_verb_key.append(ShellUtil::kRegExePath);
    run_verb_key.append(ShellUtil::kRegShellPath);
    run_verb_key.push_back(base::FilePath::kSeparators[0]);
    run_verb_key.append(ShellUtil::kRegVerbRun);
    InstallUtil::DeleteRegistryKey(root_key, run_verb_key);
  }
}

// Gets the short (8.3) form of |path|, putting the result in |short_path| and
// returning true on success.  |short_path| is not modified on failure.
bool ShortNameFromPath(const base::FilePath& path, string16* short_path) {
  DCHECK(short_path);
  string16 result(MAX_PATH, L'\0');
  DWORD short_length = GetShortPathName(path.value().c_str(), &result[0],
                                        result.size());
  if (short_length == 0 || short_length > result.size()) {
    PLOG(ERROR) << "Error getting short (8.3) path";
    return false;
  }

  result.resize(short_length);
  short_path->swap(result);
  return true;
}

// Probe using IApplicationAssociationRegistration::QueryCurrentDefault
// (Windows 8); see ProbeProtocolHandlers.  This mechanism is not suitable for
// use on previous versions of Windows despite the presence of
// QueryCurrentDefault on them since versions of Windows prior to Windows 8
// did not perform validation on the ProgID registered as the current default.
// As a result, stale ProgIDs could be returned, leading to false positives.
ShellUtil::DefaultState ProbeCurrentDefaultHandlers(
    const wchar_t* const* protocols,
    size_t num_protocols) {
  base::win::ScopedComPtr<IApplicationAssociationRegistration> registration;
  HRESULT hr = registration.CreateInstance(
      CLSID_ApplicationAssociationRegistration, NULL, CLSCTX_INPROC);
  if (FAILED(hr))
    return ShellUtil::UNKNOWN_DEFAULT;

  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  base::FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED();
    return ShellUtil::UNKNOWN_DEFAULT;
  }
  string16 prog_id(ShellUtil::kChromeHTMLProgId);
  prog_id += ShellUtil::GetCurrentInstallationSuffix(dist, chrome_exe.value());

  for (size_t i = 0; i < num_protocols; ++i) {
    base::win::ScopedCoMem<wchar_t> current_app;
    hr = registration->QueryCurrentDefault(protocols[i], AT_URLPROTOCOL,
                                           AL_EFFECTIVE, &current_app);
    if (FAILED(hr) || prog_id.compare(current_app) != 0)
      return ShellUtil::NOT_DEFAULT;
  }

  return ShellUtil::IS_DEFAULT;
}

// Probe using IApplicationAssociationRegistration::QueryAppIsDefault (Vista and
// Windows 7); see ProbeProtocolHandlers.
ShellUtil::DefaultState ProbeAppIsDefaultHandlers(
    const wchar_t* const* protocols,
    size_t num_protocols) {
  base::win::ScopedComPtr<IApplicationAssociationRegistration> registration;
  HRESULT hr = registration.CreateInstance(
      CLSID_ApplicationAssociationRegistration, NULL, CLSCTX_INPROC);
  if (FAILED(hr))
    return ShellUtil::UNKNOWN_DEFAULT;

  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  base::FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED();
    return ShellUtil::UNKNOWN_DEFAULT;
  }
  string16 app_name(ShellUtil::GetApplicationName(dist, chrome_exe.value()));

  BOOL result;
  for (size_t i = 0; i < num_protocols; ++i) {
    result = TRUE;
    hr = registration->QueryAppIsDefault(protocols[i], AT_URLPROTOCOL,
        AL_EFFECTIVE, app_name.c_str(), &result);
    if (FAILED(hr) || result == FALSE)
      return ShellUtil::NOT_DEFAULT;
  }

  return ShellUtil::IS_DEFAULT;
}

// Probe the current commands registered to handle the shell "open" verb for
// |protocols| (Windows XP); see ProbeProtocolHandlers.
ShellUtil::DefaultState ProbeOpenCommandHandlers(
    const wchar_t* const* protocols,
    size_t num_protocols) {
  // Get the path to the current exe (Chrome).
  base::FilePath app_path;
  if (!PathService::Get(base::FILE_EXE, &app_path)) {
    LOG(ERROR) << "Error getting app exe path";
    return ShellUtil::UNKNOWN_DEFAULT;
  }

  // Get its short (8.3) form.
  string16 short_app_path;
  if (!ShortNameFromPath(app_path, &short_app_path))
    return ShellUtil::UNKNOWN_DEFAULT;

  const HKEY root_key = HKEY_CLASSES_ROOT;
  string16 key_path;
  base::win::RegKey key;
  string16 value;
  CommandLine command_line(CommandLine::NO_PROGRAM);
  string16 short_path;

  for (size_t i = 0; i < num_protocols; ++i) {
    // Get the command line from HKCU\<protocol>\shell\open\command.
    key_path.assign(protocols[i]).append(ShellUtil::kRegShellOpen);
    if ((key.Open(root_key, key_path.c_str(),
                  KEY_QUERY_VALUE) != ERROR_SUCCESS) ||
        (key.ReadValue(L"", &value) != ERROR_SUCCESS)) {
      return ShellUtil::NOT_DEFAULT;
    }

    // Need to normalize path in case it's been munged.
    command_line = CommandLine::FromString(value);
    if (!ShortNameFromPath(command_line.GetProgram(), &short_path))
      return ShellUtil::UNKNOWN_DEFAULT;

    if (!base::FilePath::CompareEqualIgnoreCase(short_path, short_app_path))
      return ShellUtil::NOT_DEFAULT;
  }

  return ShellUtil::IS_DEFAULT;
}

// A helper function that probes default protocol handler registration (in a
// manner appropriate for the current version of Windows) to determine if
// Chrome is the default handler for |protocols|.  Returns IS_DEFAULT
// only if Chrome is the default for all specified protocols.
ShellUtil::DefaultState ProbeProtocolHandlers(
    const wchar_t* const* protocols,
    size_t num_protocols) {
  DCHECK(!num_protocols || protocols);
  if (DCHECK_IS_ON()) {
    for (size_t i = 0; i < num_protocols; ++i)
      DCHECK(protocols[i] && *protocols[i]);
  }

  const base::win::Version windows_version = base::win::GetVersion();

  if (windows_version >= base::win::VERSION_WIN8)
    return ProbeCurrentDefaultHandlers(protocols, num_protocols);
  else if (windows_version >= base::win::VERSION_VISTA)
    return ProbeAppIsDefaultHandlers(protocols, num_protocols);

  return ProbeOpenCommandHandlers(protocols, num_protocols);
}

// Removes shortcut at |shortcut_path| if it is a shortcut that points to
// |target_exe|. If |delete_folder| is true, deletes the parent folder of
// the shortcut completely. Returns true if either the shortcut was deleted
// successfully or if the shortcut did not point to |target_exe|.
bool MaybeRemoveShortcutAtPath(const base::FilePath& shortcut_path,
                               const base::FilePath& target_exe,
                               bool delete_folder) {
  base::FilePath target_path;
  if (!base::win::ResolveShortcut(shortcut_path, &target_path, NULL))
    return false;

  if (InstallUtil::ProgramCompare(target_exe).EvaluatePath(target_path)) {
    // Unpin the shortcut if it was ever pinned by the user or the installer.
    VLOG(1) << "Trying to unpin " << shortcut_path.value();
    if (!base::win::TaskbarUnpinShortcutLink(shortcut_path.value().c_str())) {
      VLOG(1) << shortcut_path.value()
              << " wasn't pinned (or the unpin failed).";
    }
    if (delete_folder)
      return file_util::Delete(shortcut_path.DirName(), true);
    else
      return file_util::Delete(shortcut_path, false);
  }

  // The shortcut at |shortcut_path| doesn't point to |target_exe|, act as if
  // our shortcut had been deleted.
  return true;
}

}  // namespace

const wchar_t* ShellUtil::kRegDefaultIcon = L"\\DefaultIcon";
const wchar_t* ShellUtil::kRegShellPath = L"\\shell";
const wchar_t* ShellUtil::kRegShellOpen = L"\\shell\\open\\command";
const wchar_t* ShellUtil::kRegStartMenuInternet =
    L"Software\\Clients\\StartMenuInternet";
const wchar_t* ShellUtil::kRegClasses = L"Software\\Classes";
const wchar_t* ShellUtil::kRegRegisteredApplications =
    L"Software\\RegisteredApplications";
const wchar_t* ShellUtil::kRegVistaUrlPrefs =
    L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\"
    L"http\\UserChoice";
const wchar_t* ShellUtil::kAppPathsRegistryKey =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths";
const wchar_t* ShellUtil::kAppPathsRegistryPathName = L"Path";

#if defined(GOOGLE_CHROME_BUILD)
const wchar_t* ShellUtil::kChromeHTMLProgId = L"ChromeHTML";
const wchar_t* ShellUtil::kChromeHTMLProgIdDesc = L"Chrome HTML Document";
#else
// This used to be "ChromiumHTML", but was forced to become "ChromiumHTM"
// because of http://crbug.com/153349 as with the '.' and 26 characters suffix
// added on user-level installs, the generated progid for Chromium was 39
// characters long which, according to MSDN (
// http://msdn.microsoft.com/library/aa911706.aspx), is the maximum length
// for a progid. It was however determined through experimentation that the 39
// character limit mentioned on MSDN includes the NULL character...
const wchar_t* ShellUtil::kChromeHTMLProgId = L"ChromiumHTM";
const wchar_t* ShellUtil::kChromeHTMLProgIdDesc = L"Chromium HTML Document";
#endif

const wchar_t* ShellUtil::kFileAssociations[] = {L".htm", L".html", L".shtml",
    L".xht", L".xhtml", NULL};
const wchar_t* ShellUtil::kBrowserProtocolAssociations[] = {L"ftp", L"http",
    L"https", NULL};
const wchar_t* ShellUtil::kPotentialProtocolAssociations[] = {L"ftp", L"http",
    L"https", L"irc", L"mailto", L"mms", L"news", L"nntp", L"sms", L"smsto",
    L"tel", L"urn", L"webcal", NULL};
const wchar_t* ShellUtil::kRegUrlProtocol = L"URL Protocol";
const wchar_t* ShellUtil::kRegApplication = L"\\Application";
const wchar_t* ShellUtil::kRegAppUserModelId = L"AppUserModelId";
const wchar_t* ShellUtil::kRegApplicationDescription =
    L"ApplicationDescription";
const wchar_t* ShellUtil::kRegApplicationName = L"ApplicationName";
const wchar_t* ShellUtil::kRegApplicationIcon = L"ApplicationIcon";
const wchar_t* ShellUtil::kRegApplicationCompany = L"ApplicationCompany";
const wchar_t* ShellUtil::kRegExePath = L"\\.exe";
const wchar_t* ShellUtil::kRegVerbOpen = L"open";
const wchar_t* ShellUtil::kRegVerbOpenNewWindow = L"opennewwindow";
const wchar_t* ShellUtil::kRegVerbRun = L"run";
const wchar_t* ShellUtil::kRegCommand = L"command";
const wchar_t* ShellUtil::kRegDelegateExecute = L"DelegateExecute";
const wchar_t* ShellUtil::kRegOpenWithProgids = L"OpenWithProgids";

bool ShellUtil::QuickIsChromeRegisteredInHKLM(BrowserDistribution* dist,
                                              const string16& chrome_exe,
                                              const string16& suffix) {
  return QuickIsChromeRegistered(dist, chrome_exe, suffix,
                                 CONFIRM_SHELL_REGISTRATION_IN_HKLM);
}

bool ShellUtil::GetShortcutPath(ShellUtil::ShortcutLocation location,
                                BrowserDistribution* dist,
                                ShellChange level,
                                base::FilePath* path) {
  int dir_key = -1;
  bool add_folder_for_dist = false;
  switch (location) {
    case SHORTCUT_LOCATION_DESKTOP:
      dir_key = (level == CURRENT_USER) ? base::DIR_USER_DESKTOP :
                                          base::DIR_COMMON_DESKTOP;
      break;
    case SHORTCUT_LOCATION_QUICK_LAUNCH:
      dir_key = (level == CURRENT_USER) ? base::DIR_USER_QUICK_LAUNCH :
                                          base::DIR_DEFAULT_USER_QUICK_LAUNCH;
      break;
    case SHORTCUT_LOCATION_START_MENU:
      dir_key = (level == CURRENT_USER) ? base::DIR_START_MENU :
                                          base::DIR_COMMON_START_MENU;
      add_folder_for_dist = true;
      break;
    default:
      NOTREACHED();
      return false;
  }

  if (!PathService::Get(dir_key, path) || path->empty()) {
    NOTREACHED() << dir_key;
    return false;
  }

  if (add_folder_for_dist)
    *path = path->Append(dist->GetAppShortCutName());

  return true;
}

bool ShellUtil::CreateOrUpdateShortcut(
    ShellUtil::ShortcutLocation location,
    BrowserDistribution* dist,
    const ShellUtil::ShortcutProperties& properties,
    ShellUtil::ShortcutOperation operation) {
  DCHECK(dist);
  // |pin_to_taskbar| is only acknowledged when first creating the shortcut.
  DCHECK(!properties.pin_to_taskbar ||
         operation == SHELL_SHORTCUT_CREATE_ALWAYS ||
         operation == SHELL_SHORTCUT_CREATE_IF_NO_SYSTEM_LEVEL);

  base::FilePath user_shortcut_path;
  base::FilePath system_shortcut_path;
  if (!GetShortcutPath(location, dist, SYSTEM_LEVEL, &system_shortcut_path) ||
      system_shortcut_path.empty()) {
    NOTREACHED();
    return false;
  }

  string16 shortcut_name(ExtractShortcutNameFromProperties(dist, properties));
  system_shortcut_path = system_shortcut_path.Append(shortcut_name);

  base::FilePath* chosen_path;
  bool should_install_shortcut = true;
  if (properties.level == SYSTEM_LEVEL) {
    // Install the system-level shortcut if requested.
    chosen_path = &system_shortcut_path;
  } else if (operation != SHELL_SHORTCUT_CREATE_IF_NO_SYSTEM_LEVEL ||
             !file_util::PathExists(system_shortcut_path)) {
    // Otherwise install the user-level shortcut, unless the system-level
    // variant of this shortcut is present on the machine and |operation| states
    // not to create a user-level shortcut in that case.
    if (!GetShortcutPath(location, dist, CURRENT_USER, &user_shortcut_path) ||
        user_shortcut_path.empty()) {
      NOTREACHED();
      return false;
    }
    user_shortcut_path = user_shortcut_path.Append(shortcut_name);
    chosen_path = &user_shortcut_path;
  } else {
    // Do not install any shortcut if we are told to install a user-level
    // shortcut, but the system-level variant of that shortcut is present.
    // Other actions (e.g., pinning) can still happen with respect to the
    // existing system-level shortcut however.
    chosen_path = &system_shortcut_path;
    should_install_shortcut = false;
  }

  if (chosen_path == NULL || chosen_path->empty()) {
    NOTREACHED();
    return false;
  }

  base::win::ShortcutOperation shortcut_operation =
      TranslateShortcutOperation(operation);
  bool ret = true;
  if (should_install_shortcut) {
    // Make sure the parent directories exist when creating the shortcut.
    if (shortcut_operation == base::win::SHORTCUT_CREATE_ALWAYS &&
        !file_util::CreateDirectory(chosen_path->DirName())) {
      NOTREACHED();
      return false;
    }

    base::win::ShortcutProperties shortcut_properties(
        TranslateShortcutProperties(properties));
    ret = base::win::CreateOrUpdateShortcutLink(
        *chosen_path, shortcut_properties, shortcut_operation);
  }

  if (ret && shortcut_operation == base::win::SHORTCUT_CREATE_ALWAYS &&
      properties.pin_to_taskbar &&
      base::win::GetVersion() >= base::win::VERSION_WIN7) {
    ret = base::win::TaskbarPinShortcutLink(chosen_path->value().c_str());
    if (!ret) {
      LOG(ERROR) << "Failed to pin " << chosen_path->value();
    }
  }

  return ret;
}

string16 ShellUtil::FormatIconLocation(const string16& icon_path,
                                       int icon_index) {
  string16 icon_string(icon_path);
  icon_string.append(L",");
  icon_string.append(base::IntToString16(icon_index));
  return icon_string;
}

string16 ShellUtil::GetChromeShellOpenCmd(const string16& chrome_exe) {
  return L"\"" + chrome_exe + L"\" -- \"%1\"";
}

string16 ShellUtil::GetChromeDelegateCommand(const string16& chrome_exe) {
  return L"\"" + chrome_exe + L"\" -- %*";
}

void ShellUtil::GetRegisteredBrowsers(
    BrowserDistribution* dist,
    std::map<string16, string16>* browsers) {
  DCHECK(dist);
  DCHECK(browsers);

  const string16 base_key(ShellUtil::kRegStartMenuInternet);
  string16 client_path;
  RegKey key;
  string16 name;
  string16 command;

  // HKCU has precedence over HKLM for these registrations: http://goo.gl/xjczJ.
  // Look in HKCU second to override any identical values found in HKLM.
  const HKEY roots[] = { HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER };
  for (int i = 0; i < arraysize(roots); ++i) {
    const HKEY root = roots[i];
    for (base::win::RegistryKeyIterator iter(root, base_key.c_str());
         iter.Valid(); ++iter) {
      client_path.assign(base_key).append(1, L'\\').append(iter.Name());
      // Read the browser's name (localized according to install language).
      if (key.Open(root, client_path.c_str(),
                   KEY_QUERY_VALUE) != ERROR_SUCCESS ||
          key.ReadValue(NULL, &name) != ERROR_SUCCESS ||
          name.empty() ||
          name.find(dist->GetBaseAppName()) != string16::npos) {
        continue;
      }
      // Read the browser's reinstall command.
      if (key.Open(root, (client_path + L"\\InstallInfo").c_str(),
                   KEY_QUERY_VALUE) == ERROR_SUCCESS &&
          key.ReadValue(kReinstallCommand, &command) == ERROR_SUCCESS &&
          !command.empty()) {
        (*browsers)[name] = command;
      }
    }
  }
}

string16 ShellUtil::GetCurrentInstallationSuffix(BrowserDistribution* dist,
                                                 const string16& chrome_exe) {
  // This method is somewhat the opposite of GetInstallationSpecificSuffix().
  // In this case we are not trying to determine the current suffix for the
  // upcoming installation (i.e. not trying to stick to a currently bad
  // registration style if one is present).
  // Here we want to determine which suffix we should use at run-time.
  // In order of preference, we prefer (for user-level installs):
  //   1) Base 32 encoding of the md5 hash of the user's sid (new-style).
  //   2) Username (old-style).
  //   3) Unsuffixed (even worse).
  string16 tested_suffix;
  if (InstallUtil::IsPerUserInstall(chrome_exe.c_str()) &&
      (!GetUserSpecificRegistrySuffix(&tested_suffix) ||
       !QuickIsChromeRegistered(dist, chrome_exe, tested_suffix,
                                CONFIRM_PROGID_REGISTRATION)) &&
      (!GetOldUserSpecificRegistrySuffix(&tested_suffix) ||
       !QuickIsChromeRegistered(dist, chrome_exe, tested_suffix,
                                CONFIRM_PROGID_REGISTRATION)) &&
      !QuickIsChromeRegistered(dist, chrome_exe, tested_suffix.erase(),
                               CONFIRM_PROGID_REGISTRATION)) {
    // If Chrome is not registered under any of the possible suffixes (e.g.
    // tests, Canary, etc.): use the new-style suffix at run-time.
    if (!GetUserSpecificRegistrySuffix(&tested_suffix))
      NOTREACHED();
  }
  return tested_suffix;
}

string16 ShellUtil::GetApplicationName(BrowserDistribution* dist,
                                       const string16& chrome_exe) {
  string16 app_name = dist->GetBaseAppName();
  app_name += GetCurrentInstallationSuffix(dist, chrome_exe);
  return app_name;
}

string16 ShellUtil::GetBrowserModelId(BrowserDistribution* dist,
                                      bool is_per_user_install) {
  string16 app_id(dist->GetBaseAppId());
  string16 suffix;

  // TODO(robertshield): Temporary hack to make the kRegisterChromeBrowserSuffix
  // apply to all registry values computed down in these murky depths.
  CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(
          installer::switches::kRegisterChromeBrowserSuffix)) {
    suffix = command_line.GetSwitchValueNative(
        installer::switches::kRegisterChromeBrowserSuffix);
  } else if (is_per_user_install && !GetUserSpecificRegistrySuffix(&suffix)) {
    NOTREACHED();
  }
  // There is only one component (i.e. the suffixed appid) in this case, but it
  // is still necessary to go through the appid constructor to make sure the
  // returned appid is truncated if necessary.
  std::vector<string16> components(1, app_id.append(suffix));
  return BuildAppModelId(components);
}

string16 ShellUtil::BuildAppModelId(
    const std::vector<string16>& components) {
  DCHECK_GT(components.size(), 0U);

  // Find the maximum numbers of characters allowed in each component
  // (accounting for the dots added between each component).
  const size_t available_chars =
      installer::kMaxAppModelIdLength - (components.size() - 1);
  const size_t max_component_length = available_chars / components.size();

  // |max_component_length| should be at least 2; otherwise the truncation logic
  // below breaks.
  if (max_component_length < 2U) {
    NOTREACHED();
    return (*components.begin()).substr(0, installer::kMaxAppModelIdLength);
  }

  string16 app_id;
  app_id.reserve(installer::kMaxAppModelIdLength);
  for (std::vector<string16>::const_iterator it = components.begin();
       it != components.end(); ++it) {
    if (it != components.begin())
      app_id.push_back(L'.');

    const string16& component = *it;
    DCHECK(!component.empty());
    if (component.length() > max_component_length) {
      // Append a shortened version of this component. Cut in the middle to try
      // to avoid losing the unique parts of this component (which are usually
      // at the beginning or end for things like usernames and paths).
      app_id.append(component.c_str(), 0, max_component_length / 2);
      app_id.append(component.c_str(),
                    component.length() - ((max_component_length + 1) / 2),
                    string16::npos);
    } else {
      app_id.append(component);
    }
  }
  // No spaces are allowed in the AppUserModelId according to MSDN.
  ReplaceChars(app_id, L" ", L"_", &app_id);
  return app_id;
}

ShellUtil::DefaultState ShellUtil::GetChromeDefaultState() {
  // When we check for default browser we don't necessarily want to count file
  // type handlers and icons as having changed the default browser status,
  // since the user may have changed their shell settings to cause HTML files
  // to open with a text editor for example. We also don't want to aggressively
  // claim FTP, since the user may have a separate FTP client. It is an open
  // question as to how to "heal" these settings. Perhaps the user should just
  // re-run the installer or run with the --set-default-browser command line
  // flag. There is doubtless some other key we can hook into to cause "Repair"
  // to show up in Add/Remove programs for us.
  static const wchar_t* const kChromeProtocols[] = { L"http", L"https" };
  return ProbeProtocolHandlers(kChromeProtocols, arraysize(kChromeProtocols));
}

ShellUtil::DefaultState ShellUtil::GetChromeDefaultProtocolClientState(
    const string16& protocol) {
  if (protocol.empty())
    return UNKNOWN_DEFAULT;

  const wchar_t* const protocols[] = { protocol.c_str() };
  return ProbeProtocolHandlers(protocols, arraysize(protocols));
}

// static
bool ShellUtil::CanMakeChromeDefaultUnattended() {
  return base::win::GetVersion() < base::win::VERSION_WIN8;
}

bool ShellUtil::MakeChromeDefault(BrowserDistribution* dist,
                                  int shell_change,
                                  const string16& chrome_exe,
                                  bool elevate_if_not_admin) {
  DCHECK(!(shell_change & ShellUtil::SYSTEM_LEVEL) || IsUserAnAdmin());

  if (!dist->CanSetAsDefault())
    return false;

  // Windows 8 does not permit making a browser default just like that.
  // This process needs to be routed through the system's UI. Use
  // ShowMakeChromeDefaultSystemUI instead (below).
  if (!CanMakeChromeDefaultUnattended()) {
    return false;
  }

  if (!ShellUtil::RegisterChromeBrowser(
          dist, chrome_exe, string16(), elevate_if_not_admin)) {
    return false;
  }

  bool ret = true;
  // First use the new "recommended" way on Vista to make Chrome default
  // browser.
  string16 app_name = GetApplicationName(dist, chrome_exe);

  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    // On Windows Vista and Win7 we still can set ourselves via the
    // the IApplicationAssociationRegistration interface.
    VLOG(1) << "Registering Chrome as default browser on Vista.";
    base::win::ScopedComPtr<IApplicationAssociationRegistration> pAAR;
    HRESULT hr = pAAR.CreateInstance(CLSID_ApplicationAssociationRegistration,
        NULL, CLSCTX_INPROC);
    if (SUCCEEDED(hr)) {
      for (int i = 0; ShellUtil::kBrowserProtocolAssociations[i] != NULL; i++) {
        hr = pAAR->SetAppAsDefault(app_name.c_str(),
            ShellUtil::kBrowserProtocolAssociations[i], AT_URLPROTOCOL);
        if (!SUCCEEDED(hr)) {
          ret = false;
          LOG(ERROR) << "Failed to register as default for protocol "
                     << ShellUtil::kBrowserProtocolAssociations[i]
                     << " (" << hr << ")";
        }
      }

      for (int i = 0; ShellUtil::kFileAssociations[i] != NULL; i++) {
        hr = pAAR->SetAppAsDefault(app_name.c_str(),
            ShellUtil::kFileAssociations[i], AT_FILEEXTENSION);
        if (!SUCCEEDED(hr)) {
          ret = false;
          LOG(ERROR) << "Failed to register as default for file extension "
                     << ShellUtil::kFileAssociations[i] << " (" << hr << ")";
        }
      }
    }
  }

  if (!RegisterChromeAsDefaultXPStyle(dist, shell_change, chrome_exe))
    ret = false;

  // Send Windows notification event so that it can update icons for
  // file associations.
  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
  return ret;
}

bool ShellUtil::ShowMakeChromeDefaultSystemUI(BrowserDistribution* dist,
                                              const string16& chrome_exe) {
  DCHECK_GE(base::win::GetVersion(), base::win::VERSION_WIN8);
  if (!dist->CanSetAsDefault())
    return false;

  if (!RegisterChromeBrowser(dist, chrome_exe, string16(), true))
      return false;

  bool succeeded = true;
  bool is_default = (GetChromeDefaultState() == IS_DEFAULT);
  if (!is_default) {
    // On Windows 8, you can't set yourself as the default handler
    // programatically. In other words IApplicationAssociationRegistration
    // has been rendered useless. What you can do is to launch
    // "Set Program Associations" section of the "Default Programs"
    // control panel, which is a mess, or pop the concise "How you want to open
    // webpages?" dialog.  We choose the latter.
    succeeded = LaunchSelectDefaultProtocolHandlerDialog(L"http");
    is_default = (succeeded && GetChromeDefaultState() == IS_DEFAULT);
  }
  if (succeeded && is_default)
    RegisterChromeAsDefaultXPStyle(dist, CURRENT_USER, chrome_exe);
  return succeeded;
}

bool ShellUtil::MakeChromeDefaultProtocolClient(BrowserDistribution* dist,
                                                const string16& chrome_exe,
                                                const string16& protocol) {
  if (!dist->CanSetAsDefault())
    return false;

  if (!RegisterChromeForProtocol(dist, chrome_exe, string16(), protocol, true))
    return false;

  // Windows 8 does not permit making a browser default just like that.
  // This process needs to be routed through the system's UI. Use
  // ShowMakeChromeDefaultProocolClientSystemUI instead (below).
  if (!CanMakeChromeDefaultUnattended())
    return false;

  bool ret = true;
  // First use the new "recommended" way on Vista to make Chrome default
  // protocol handler.
  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    VLOG(1) << "Registering Chrome as default handler for " << protocol
            << " on Vista.";
    base::win::ScopedComPtr<IApplicationAssociationRegistration> pAAR;
    HRESULT hr = pAAR.CreateInstance(CLSID_ApplicationAssociationRegistration,
      NULL, CLSCTX_INPROC);
    if (SUCCEEDED(hr)) {
      string16 app_name = GetApplicationName(dist, chrome_exe);
      hr = pAAR->SetAppAsDefault(app_name.c_str(), protocol.c_str(),
                                 AT_URLPROTOCOL);
    }
    if (!SUCCEEDED(hr)) {
      ret = false;
      LOG(ERROR) << "Could not make Chrome default protocol client (Vista):"
                 << " HRESULT=" << hr << ".";
    }
  }

  // Now use the old way to associate Chrome with the desired protocol. This
  // should not be required on Vista+, but since some applications still read
  // Software\Classes\<protocol> key directly, do this on Vista+ also.
  if (!RegisterChromeAsDefaultProtocolClientXPStyle(dist, chrome_exe, protocol))
    ret = false;

  return ret;
}

bool ShellUtil::ShowMakeChromeDefaultProtocolClientSystemUI(
    BrowserDistribution* dist,
    const string16& chrome_exe,
    const string16& protocol) {
  DCHECK_GE(base::win::GetVersion(), base::win::VERSION_WIN8);
  if (!dist->CanSetAsDefault())
    return false;

  if (!RegisterChromeForProtocol(dist, chrome_exe, string16(), protocol, true))
    return false;

  bool succeeded = true;
  bool is_default = (
      GetChromeDefaultProtocolClientState(protocol) == IS_DEFAULT);
  if (!is_default) {
    // On Windows 8, you can't set yourself as the default handler
    // programatically. In other words IApplicationAssociationRegistration
    // has been rendered useless. What you can do is to launch
    // "Set Program Associations" section of the "Default Programs"
    // control panel, which is a mess, or pop the concise "How you want to open
    // links of this type (protocol)?" dialog.  We choose the latter.
    succeeded = LaunchSelectDefaultProtocolHandlerDialog(protocol.c_str());
    is_default = (succeeded &&
                  GetChromeDefaultProtocolClientState(protocol) == IS_DEFAULT);
  }
  if (succeeded && is_default)
    RegisterChromeAsDefaultProtocolClientXPStyle(dist, chrome_exe, protocol);
  return succeeded;
}

bool ShellUtil::RegisterChromeBrowser(BrowserDistribution* dist,
                                      const string16& chrome_exe,
                                      const string16& unique_suffix,
                                      bool elevate_if_not_admin) {
  if (!dist->CanSetAsDefault())
    return false;

  CommandLine& command_line = *CommandLine::ForCurrentProcess();

  string16 suffix;
  if (!unique_suffix.empty()) {
    suffix = unique_suffix;
  } else if (command_line.HasSwitch(
                 installer::switches::kRegisterChromeBrowserSuffix)) {
    suffix = command_line.GetSwitchValueNative(
        installer::switches::kRegisterChromeBrowserSuffix);
  } else if (!GetInstallationSpecificSuffix(dist, chrome_exe, &suffix)) {
    return false;
  }

  RemoveRunVerbOnWindows8(dist, chrome_exe);

  // Check if Chromium is already registered with this suffix.
  if (IsChromeRegistered(dist, chrome_exe, suffix))
    return true;

  bool user_level = InstallUtil::IsPerUserInstall(chrome_exe.c_str());
  HKEY root = DetermineRegistrationRoot(user_level);

  bool result = true;
  if (root == HKEY_CURRENT_USER || IsUserAnAdmin()) {
    // Do the full registration if we can do it at user-level or if the user is
    // an admin.
    ScopedVector<RegistryEntry> progid_and_appreg_entries;
    ScopedVector<RegistryEntry> shell_entries;
    RegistryEntry::GetProgIdEntries(dist, chrome_exe, suffix,
                                    &progid_and_appreg_entries);
    RegistryEntry::GetAppRegistrationEntries(chrome_exe, suffix,
                                             &progid_and_appreg_entries);
    RegistryEntry::GetShellIntegrationEntries(
        dist, chrome_exe, suffix, &shell_entries);
    result = (AddRegistryEntries(root, progid_and_appreg_entries) &&
              AddRegistryEntries(root, shell_entries));
  } else if (elevate_if_not_admin &&
             base::win::GetVersion() >= base::win::VERSION_VISTA &&
             ElevateAndRegisterChrome(dist, chrome_exe, suffix, L"")) {
    // If the user is not an admin and OS is between Vista and Windows 7
    // inclusively, try to elevate and register. This is only intended for
    // user-level installs as system-level installs should always be run with
    // admin rights.
    result = true;
  } else {
    // If we got to this point then all we can do is create ProgId and basic app
    // registrations under HKCU.
    ScopedVector<RegistryEntry> entries;
    RegistryEntry::GetProgIdEntries(dist, chrome_exe, string16(), &entries);
    // Prefer to use |suffix|; unless Chrome's ProgIds are already registered
    // with no suffix (as per the old registration style): in which case some
    // other registry entries could refer to them and since we were not able to
    // set our HKLM entries above, we are better off not altering these here.
    if (!AreEntriesRegistered(entries, RegistryEntry::LOOK_IN_HKCU)) {
      if (!suffix.empty()) {
        entries.clear();
        RegistryEntry::GetProgIdEntries(dist, chrome_exe, suffix, &entries);
        RegistryEntry::GetAppRegistrationEntries(chrome_exe, suffix, &entries);
      }
      result = AddRegistryEntries(HKEY_CURRENT_USER, entries);
    } else {
      // The ProgId is registered unsuffixed in HKCU, also register the app with
      // Windows in HKCU (this was not done in the old registration style and
      // thus needs to be done after the above check for the unsuffixed
      // registration).
      entries.clear();
      RegistryEntry::GetAppRegistrationEntries(chrome_exe, string16(),
                                               &entries);
      result = AddRegistryEntries(HKEY_CURRENT_USER, entries);
    }
  }
  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
  return result;
}

bool ShellUtil::RegisterChromeForProtocol(BrowserDistribution* dist,
                                          const string16& chrome_exe,
                                          const string16& unique_suffix,
                                          const string16& protocol,
                                          bool elevate_if_not_admin) {
  if (!dist->CanSetAsDefault())
    return false;

  string16 suffix;
  if (!unique_suffix.empty()) {
    suffix = unique_suffix;
  } else if (!GetInstallationSpecificSuffix(dist, chrome_exe, &suffix)) {
    return false;
  }

  // Check if Chromium is already registered with this suffix.
  if (IsChromeRegisteredForProtocol(dist, suffix, protocol))
    return true;

  HKEY root = DetermineRegistrationRoot(
      InstallUtil::IsPerUserInstall(chrome_exe.c_str()));

  if (root == HKEY_CURRENT_USER || IsUserAnAdmin()) {
    // We can do this operation directly.
    // First, make sure Chrome is fully registered on this machine.
    if (!RegisterChromeBrowser(dist, chrome_exe, suffix, false))
      return false;

    // Write in the capabillity for the protocol.
    ScopedVector<RegistryEntry> entries;
    RegistryEntry::GetProtocolCapabilityEntries(dist, suffix, protocol,
                                                &entries);
    return AddRegistryEntries(root, entries);
  } else if (elevate_if_not_admin &&
             base::win::GetVersion() >= base::win::VERSION_VISTA) {
    // Elevate to do the whole job
    return ElevateAndRegisterChrome(dist, chrome_exe, suffix, protocol);
  } else {
    // Admin rights are required to register capabilities before Windows 8.
    return false;
  }
}

bool ShellUtil::RemoveShortcut(ShellUtil::ShortcutLocation location,
                               BrowserDistribution* dist,
                               const base::FilePath& target_exe,
                               ShellChange level,
                               const string16* shortcut_name) {
  const bool delete_folder = (location == SHORTCUT_LOCATION_START_MENU);

  base::FilePath shortcut_folder;
  if (!GetShortcutPath(location, dist, level, &shortcut_folder) ||
      shortcut_folder.empty()) {
    NOTREACHED();
    return false;
  }

  if (!delete_folder && !shortcut_name) {
    file_util::FileEnumerator enumerator(shortcut_folder, false,
        file_util::FileEnumerator::FILES);
    bool had_failures = false;
    for (base::FilePath path = enumerator.Next(); !path.empty();
         path = enumerator.Next()) {
      if (path.Extension() != installer::kLnkExt)
        continue;

      if (!MaybeRemoveShortcutAtPath(path, target_exe, delete_folder))
        had_failures = true;
    }
    return !had_failures;
  }

  const string16 shortcut_base_name(
      (shortcut_name ? *shortcut_name : dist->GetAppShortCutName()) +
      installer::kLnkExt);
  const base::FilePath shortcut_path(
      shortcut_folder.Append(shortcut_base_name));
  if (!file_util::PathExists(shortcut_path))
    return true;

  return MaybeRemoveShortcutAtPath(shortcut_path, target_exe, delete_folder);
}

void ShellUtil::RemoveTaskbarShortcuts(const string16& target_exe) {
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  base::FilePath taskbar_pins_path;
  if (!PathService::Get(base::DIR_TASKBAR_PINS, &taskbar_pins_path) ||
      !file_util::PathExists(taskbar_pins_path)) {
    LOG(ERROR) << "Couldn't find path to taskbar pins.";
    return;
  }

  file_util::FileEnumerator shortcuts_enum(
      taskbar_pins_path, false,
      file_util::FileEnumerator::FILES, FILE_PATH_LITERAL("*.lnk"));

  base::FilePath target_path(target_exe);
  InstallUtil::ProgramCompare target_compare(target_path);
  for (base::FilePath shortcut_path = shortcuts_enum.Next();
       !shortcut_path.empty();
       shortcut_path = shortcuts_enum.Next()) {
    base::FilePath read_target;
    if (!base::win::ResolveShortcut(shortcut_path, &read_target, NULL)) {
      LOG(ERROR) << "Couldn't resolve shortcut at " << shortcut_path.value();
      continue;
    }
    if (target_compare.EvaluatePath(read_target)) {
      // Unpin this shortcut if it points to |target_exe|.
      base::win::TaskbarUnpinShortcutLink(shortcut_path.value().c_str());
    }
  }
}

void ShellUtil::RemoveStartScreenShortcuts(BrowserDistribution* dist,
                                           const string16& target_exe) {
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    return;

  base::FilePath app_shortcuts_path;
  if (!PathService::Get(base::DIR_APP_SHORTCUTS, &app_shortcuts_path)) {
    LOG(ERROR) << "Could not get application shortcuts location to delete"
               << " start screen shortcuts.";
    return;
  }

  app_shortcuts_path = app_shortcuts_path.Append(
      GetBrowserModelId(dist,
                        InstallUtil::IsPerUserInstall(target_exe.c_str())));
  if (!file_util::DirectoryExists(app_shortcuts_path)) {
    VLOG(1) << "No start screen shortcuts to delete.";
    return;
  }

  VLOG(1) << "Removing start screen shortcuts from "
          << app_shortcuts_path.value();
  if (!file_util::Delete(app_shortcuts_path, true)) {
    LOG(ERROR) << "Failed to remove start screen shortcuts from "
               << app_shortcuts_path.value();
  }
}

bool ShellUtil::GetUserSpecificRegistrySuffix(string16* suffix) {
  // Use a thread-safe cache for the user's suffix.
  static base::LazyInstance<UserSpecificRegistrySuffix>::Leaky suffix_instance =
      LAZY_INSTANCE_INITIALIZER;
  return suffix_instance.Get().GetSuffix(suffix);
}

bool ShellUtil::GetOldUserSpecificRegistrySuffix(string16* suffix) {
  wchar_t user_name[256];
  DWORD size = arraysize(user_name);
  if (::GetUserName(user_name, &size) == 0 || size < 1) {
    NOTREACHED();
    return false;
  }
  suffix->reserve(size);
  suffix->assign(1, L'.');
  suffix->append(user_name, size - 1);
  return true;
}

string16 ShellUtil::ByteArrayToBase32(const uint8* bytes, size_t size) {
  static const char kEncoding[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

  // Eliminate special cases first.
  if (size == 0) {
    return string16();
  } else if (size == 1) {
    string16 ret;
    ret.push_back(kEncoding[(bytes[0] & 0xf8) >> 3]);
    ret.push_back(kEncoding[(bytes[0] & 0x07) << 2]);
    return ret;
  } else if (size >= std::numeric_limits<size_t>::max() / 8) {
    // If |size| is too big, the calculation of |encoded_length| below will
    // overflow.
    NOTREACHED();
    return string16();
  }

  // Overestimate the number of bits in the string by 4 so that dividing by 5
  // is the equivalent of rounding up the actual number of bits divided by 5.
  const size_t encoded_length = (size * 8 + 4) / 5;

  string16 ret;
  ret.reserve(encoded_length);

  // A bit stream which will be read from the left and appended to from the
  // right as it's emptied.
  uint16 bit_stream = (bytes[0] << 8) + bytes[1];
  size_t next_byte_index = 2;
  int free_bits = 0;
  while (free_bits < 16) {
    // Extract the 5 leftmost bits in the stream
    ret.push_back(kEncoding[(bit_stream & 0xf800) >> 11]);
    bit_stream <<= 5;
    free_bits += 5;

    // If there is enough room in the bit stream, inject another byte (if there
    // are any left...).
    if (free_bits >= 8 && next_byte_index < size) {
      free_bits -= 8;
      bit_stream += bytes[next_byte_index++] << free_bits;
    }
  }

  DCHECK_EQ(ret.length(), encoded_length);
  return ret;
}
