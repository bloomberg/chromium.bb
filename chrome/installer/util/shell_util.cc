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

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/cancellation_flag.h"
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
#include "chrome/installer/util/work_item.h"

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
base::string16 GetBrowserProgId(const base::string16& suffix) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  base::string16 chrome_html(dist->GetBrowserProgIdPrefix());
  chrome_html.append(suffix);

  // ProgIds cannot be longer than 39 characters.
  // Ref: http://msdn.microsoft.com/en-us/library/aa911706.aspx.
  // Make all new registrations comply with this requirement (existing
  // registrations must be preserved).
  base::string16 new_style_suffix;
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
  bool GetSuffix(base::string16* suffix);

 private:
  base::string16 suffix_;

  DISALLOW_COPY_AND_ASSIGN(UserSpecificRegistrySuffix);
};  // class UserSpecificRegistrySuffix

UserSpecificRegistrySuffix::UserSpecificRegistrySuffix() {
  base::string16 user_sid;
  if (!base::win::GetUserSidString(&user_sid)) {
    NOTREACHED();
    return;
  }
  COMPILE_ASSERT(sizeof(base::MD5Digest) == 16, size_of_MD5_not_as_expected_);
  base::MD5Digest md5_digest;
  std::string user_sid_ascii(base::UTF16ToASCII(user_sid));
  base::MD5Sum(user_sid_ascii.c_str(), user_sid_ascii.length(), &md5_digest);
  const base::string16 base32_md5(
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

bool UserSpecificRegistrySuffix::GetSuffix(base::string16* suffix) {
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
  static base::string16 GetBrowserClientKey(BrowserDistribution* dist,
                                            const base::string16& suffix) {
    DCHECK(suffix.empty() || suffix[0] == L'.');
    return base::string16(ShellUtil::kRegStartMenuInternet)
        .append(1, L'\\')
        .append(dist->GetBaseAppName())
        .append(suffix);
  }

  // Returns the Windows Default Programs capabilities key for Chrome.  For
  // example:
  // "Software\Clients\StartMenuInternet\Chromium[.user]\Capabilities".
  static base::string16 GetCapabilitiesKey(BrowserDistribution* dist,
                                           const base::string16& suffix) {
    return GetBrowserClientKey(dist, suffix).append(L"\\Capabilities");
  }

  // This method returns a list of all the registry entries that
  // are needed to register this installation's ProgId and AppId.
  // These entries need to be registered in HKLM prior to Win8.
  static void GetProgIdEntries(BrowserDistribution* dist,
                               const base::string16& chrome_exe,
                               const base::string16& suffix,
                               ScopedVector<RegistryEntry>* entries) {
    base::string16 icon_path(
        ShellUtil::FormatIconLocation(
            chrome_exe,
            dist->GetIconIndex(BrowserDistribution::SHORTCUT_CHROME)));
    base::string16 open_cmd(ShellUtil::GetChromeShellOpenCmd(chrome_exe));
    base::string16 delegate_command(
        ShellUtil::GetChromeDelegateCommand(chrome_exe));
    // For user-level installs: entries for the app id and DelegateExecute verb
    // handler will be in HKCU; thus we do not need a suffix on those entries.
    base::string16 app_id(
        ShellUtil::GetBrowserModelId(
            dist, InstallUtil::IsPerUserInstall(chrome_exe.c_str())));
    base::string16 delegate_guid;
    bool set_delegate_execute =
        IsChromeMetroSupported() &&
        dist->GetCommandExecuteImplClsid(&delegate_guid);

    // DelegateExecute ProgId. Needed for Chrome Metro in Windows 8.
    if (set_delegate_execute) {
      base::string16 model_id_shell(ShellUtil::kRegClasses);
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
        base::string16 sub_path(model_id_shell);
        sub_path.push_back(base::FilePath::kSeparators[0]);
        sub_path.append(verbs[i].verb);

        // <root hkey>\Software\Classes\<app_id>\.exe\shell\<verb>
        if (verbs[i].name_id != -1) {
          // TODO(grt): http://crbug.com/75152 Write a reference to a localized
          // resource.
          base::string16 verb_name(
              installer::GetLocalizedString(verbs[i].name_id));
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
    base::string16 chrome_html_prog_id(ShellUtil::kRegClasses);
    chrome_html_prog_id.push_back(base::FilePath::kSeparators[0]);
    chrome_html_prog_id.append(GetBrowserProgId(suffix));
    entries->push_back(new RegistryEntry(
        chrome_html_prog_id, dist->GetBrowserProgIdDesc()));
    entries->push_back(new RegistryEntry(
        chrome_html_prog_id, ShellUtil::kRegUrlProtocol, base::string16()));
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
      base::string16 chrome_application(chrome_html_prog_id +
                                        ShellUtil::kRegApplication);
      entries->push_back(new RegistryEntry(
          chrome_application, ShellUtil::kRegAppUserModelId, app_id));
      entries->push_back(new RegistryEntry(
          chrome_application, ShellUtil::kRegApplicationIcon, icon_path));
      // TODO(grt): http://crbug.com/75152 Write a reference to a localized
      // resource for name, description, and company.
      entries->push_back(new RegistryEntry(
          chrome_application, ShellUtil::kRegApplicationName,
          dist->GetDisplayName()));
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
      const base::string16& suffix,
      const base::string16& protocol,
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
                                         const base::string16& chrome_exe,
                                         const base::string16& suffix,
                                         ScopedVector<RegistryEntry>* entries) {
    const base::string16 icon_path(
        ShellUtil::FormatIconLocation(
            chrome_exe,
            dist->GetIconIndex(BrowserDistribution::SHORTCUT_CHROME)));
    const base::string16 quoted_exe_path(L"\"" + chrome_exe + L"\"");

    // Register for the Start Menu "Internet" link (pre-Win7).
    const base::string16 start_menu_entry(GetBrowserClientKey(dist, suffix));
    // Register Chrome's display name.
    // TODO(grt): http://crbug.com/75152 Also set LocalizedString; see
    // http://msdn.microsoft.com/en-us/library/windows/desktop/cc144109(v=VS.85).aspx#registering_the_display_name
    entries->push_back(new RegistryEntry(
        start_menu_entry, dist->GetDisplayName()));
    // Register the "open" verb for launching Chrome via the "Internet" link.
    entries->push_back(new RegistryEntry(
        start_menu_entry + ShellUtil::kRegShellOpen, quoted_exe_path));
    // Register Chrome's icon for the Start Menu "Internet" link.
    entries->push_back(new RegistryEntry(
        start_menu_entry + ShellUtil::kRegDefaultIcon, icon_path));

    // Register installation information.
    base::string16 install_info(start_menu_entry + L"\\InstallInfo");
    // Note: not using CommandLine since it has ambiguous rules for quoting
    // strings.
    entries->push_back(new RegistryEntry(install_info, kReinstallCommand,
        quoted_exe_path + L" --" +
        base::ASCIIToWide(switches::kMakeDefaultBrowser)));
    entries->push_back(new RegistryEntry(install_info, L"HideIconsCommand",
        quoted_exe_path + L" --" +
        base::ASCIIToWide(switches::kHideIcons)));
    entries->push_back(new RegistryEntry(install_info, L"ShowIconsCommand",
        quoted_exe_path + L" --" +
        base::ASCIIToWide(switches::kShowIcons)));
    entries->push_back(new RegistryEntry(install_info, L"IconsVisible", 1));

    // Register with Default Programs.
    const base::string16 reg_app_name(dist->GetBaseAppName().append(suffix));
    // Tell Windows where to find Chrome's Default Programs info.
    const base::string16 capabilities(GetCapabilitiesKey(dist, suffix));
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
        dist->GetDisplayName()));

    entries->push_back(new RegistryEntry(capabilities + L"\\Startmenu",
        L"StartMenuInternet", reg_app_name));

    const base::string16 html_prog_id(GetBrowserProgId(suffix));
    for (int i = 0; ShellUtil::kPotentialFileAssociations[i] != NULL; i++) {
      entries->push_back(new RegistryEntry(
          capabilities + L"\\FileAssociations",
          ShellUtil::kPotentialFileAssociations[i], html_prog_id));
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
  static void GetAppRegistrationEntries(const base::string16& chrome_exe,
                                        const base::string16& suffix,
                                        ScopedVector<RegistryEntry>* entries) {
    const base::FilePath chrome_path(chrome_exe);
    base::string16 app_path_key(ShellUtil::kAppPathsRegistryKey);
    app_path_key.push_back(base::FilePath::kSeparators[0]);
    app_path_key.append(chrome_path.BaseName().value());
    entries->push_back(new RegistryEntry(app_path_key, chrome_exe));
    entries->push_back(new RegistryEntry(app_path_key,
        ShellUtil::kAppPathsRegistryPathName, chrome_path.DirName().value()));

    const base::string16 html_prog_id(GetBrowserProgId(suffix));
    for (int i = 0; ShellUtil::kPotentialFileAssociations[i] != NULL; i++) {
      base::string16 key(ShellUtil::kRegClasses);
      key.push_back(base::FilePath::kSeparators[0]);
      key.append(ShellUtil::kPotentialFileAssociations[i]);
      key.push_back(base::FilePath::kSeparators[0]);
      key.append(ShellUtil::kRegOpenWithProgids);
      entries->push_back(
          new RegistryEntry(key, html_prog_id, base::string16()));
    }
  }

  // This method returns a list of all the user level registry entries that
  // are needed to make Chromium the default handler for a protocol on XP.
  static void GetXPStyleUserProtocolEntries(
      const base::string16& protocol,
      const base::string16& chrome_icon,
      const base::string16& chrome_open,
      ScopedVector<RegistryEntry>* entries) {
    // Protocols associations.
    base::string16 url_key(ShellUtil::kRegClasses);
    url_key.push_back(base::FilePath::kSeparators[0]);
    url_key.append(protocol);

    // This registry value tells Windows that this 'class' is a URL scheme
    // so IE, explorer and other apps will route it to our handler.
    // <root hkey>\Software\Classes\<protocol>\URL Protocol
    entries->push_back(new RegistryEntry(url_key,
        ShellUtil::kRegUrlProtocol, base::string16()));

    // <root hkey>\Software\Classes\<protocol>\DefaultIcon
    base::string16 icon_key = url_key + ShellUtil::kRegDefaultIcon;
    entries->push_back(new RegistryEntry(icon_key, chrome_icon));

    // <root hkey>\Software\Classes\<protocol>\shell\open\command
    base::string16 shell_key = url_key + ShellUtil::kRegShellOpen;
    entries->push_back(new RegistryEntry(shell_key, chrome_open));

    // <root hkey>\Software\Classes\<protocol>\shell\open\ddeexec
    base::string16 dde_key = url_key + L"\\shell\\open\\ddeexec";
    entries->push_back(new RegistryEntry(dde_key, base::string16()));

    // <root hkey>\Software\Classes\<protocol>\shell\@
    base::string16 protocol_shell_key = url_key + ShellUtil::kRegShellPath;
    entries->push_back(new RegistryEntry(protocol_shell_key, L"open"));
  }

  // This method returns a list of all the user level registry entries that
  // are needed to make Chromium default browser on XP.
  // Some of these entries are irrelevant in recent versions of Windows, but
  // we register them anyways as some legacy apps are hardcoded to lookup those
  // values.
  static void GetXPStyleDefaultBrowserUserEntries(
      BrowserDistribution* dist,
      const base::string16& chrome_exe,
      const base::string16& suffix,
      ScopedVector<RegistryEntry>* entries) {
    // File extension associations.
    base::string16 html_prog_id(GetBrowserProgId(suffix));
    for (int i = 0; ShellUtil::kDefaultFileAssociations[i] != NULL; i++) {
      base::string16 ext_key(ShellUtil::kRegClasses);
      ext_key.push_back(base::FilePath::kSeparators[0]);
      ext_key.append(ShellUtil::kDefaultFileAssociations[i]);
      entries->push_back(new RegistryEntry(ext_key, html_prog_id));
    }

    // Protocols associations.
    base::string16 chrome_open = ShellUtil::GetChromeShellOpenCmd(chrome_exe);
    base::string16 chrome_icon =
        ShellUtil::FormatIconLocation(
            chrome_exe,
            dist->GetIconIndex(BrowserDistribution::SHORTCUT_CHROME));
    for (int i = 0; ShellUtil::kBrowserProtocolAssociations[i] != NULL; i++) {
      GetXPStyleUserProtocolEntries(ShellUtil::kBrowserProtocolAssociations[i],
                                    chrome_icon, chrome_open, entries);
    }

    // start->Internet shortcut.
    base::string16 start_menu(ShellUtil::kRegStartMenuInternet);
    base::string16 app_name = dist->GetBaseAppName() + suffix;
    entries->push_back(new RegistryEntry(start_menu, app_name));
  }

  // Generate work_item tasks required to create current registry entry and
  // add them to the given work item list.
  void AddToWorkItemList(HKEY root, WorkItemList *items) const {
    items->AddCreateRegKeyWorkItem(root, key_path_, WorkItem::kWow64Default);
    if (is_string_) {
      items->AddSetRegValueWorkItem(
          root, key_path_, WorkItem::kWow64Default, name_, value_, true);
    } else {
      items->AddSetRegValueWorkItem(
          root, key_path_, WorkItem::kWow64Default, name_, int_value_, true);
    }
  }

  // Checks if the current registry entry exists in HKCU\|key_path_|\|name_|
  // and value is |value_|. If the key does NOT exist in HKCU, checks for
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
    // |name_| does not exist in the registry
    DOES_NOT_EXIST,
    // |name_| exists, but its value != |value_|
    DIFFERENT_VALUE,
    // |name_| exists and its value is |value_|
    SAME_VALUE,
  };

  // Create a object that represent default value of a key
  RegistryEntry(const base::string16& key_path, const base::string16& value)
      : key_path_(key_path), name_(),
        is_string_(true), value_(value), int_value_(0) {
  }

  // Create a object that represent a key of type REG_SZ
  RegistryEntry(const base::string16& key_path, const base::string16& name,
                const base::string16& value)
      : key_path_(key_path), name_(name),
        is_string_(true), value_(value), int_value_(0) {
  }

  // Create a object that represent a key of integer type
  RegistryEntry(const base::string16& key_path, const base::string16& name,
                DWORD value)
      : key_path_(key_path), name_(name),
        is_string_(false), value_(), int_value_(value) {
  }

  base::string16 key_path_;  // key path for the registry entry
  base::string16 name_;      // name of the registry entry
  bool is_string_;     // true if current registry entry is of type REG_SZ
  base::string16 value_;     // string value (useful if is_string_ = true)
  DWORD int_value_;    // integer value (useful if is_string_ = false)

  // Helper function for ExistsInRegistry().
  // Returns the RegistryStatus of the current registry entry in
  // |root|\|key_path_|\|name_|.
  RegistryStatus StatusInRegistryUnderRoot(HKEY root) const {
    RegKey key(root, key_path_.c_str(), KEY_QUERY_VALUE);
    bool found = false;
    bool correct_value = false;
    if (is_string_) {
      base::string16 read_value;
      found = key.ReadValue(name_.c_str(), &read_value) == ERROR_SUCCESS;
      if (found) {
        correct_value = read_value.size() == value_.size() &&
            std::equal(value_.begin(), value_.end(), read_value.begin(),
                       base::CaseInsensitiveCompare<wchar_t>());
      }
    } else {
      DWORD read_value;
      found = key.ReadValueDW(name_.c_str(), &read_value) == ERROR_SUCCESS;
      if (found)
        correct_value = read_value == int_value_;
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
// on this computer. See RegistryEntry::ExistsInRegistry for the behavior of
// |look_for_in|.
// Note: between r133333 and r154145 we were registering parts of Chrome in HKCU
// and parts in HKLM for user-level installs; we now always register everything
// under a single registry root. Not doing so caused http://crbug.com/144910 for
// users who first-installed Chrome in that revision range (those users are
// still impacted by http://crbug.com/144910). This method will keep returning
// true for affected users (i.e. who have all the registrations, but over both
// registry roots).
bool IsChromeRegistered(BrowserDistribution* dist,
                        const base::string16& chrome_exe,
                        const base::string16& suffix,
                        uint32 look_for_in) {
  ScopedVector<RegistryEntry> entries;
  RegistryEntry::GetProgIdEntries(dist, chrome_exe, suffix, &entries);
  RegistryEntry::GetShellIntegrationEntries(dist, chrome_exe, suffix, &entries);
  RegistryEntry::GetAppRegistrationEntries(chrome_exe, suffix, &entries);
  return AreEntriesRegistered(entries, look_for_in);
}

// This method checks if Chrome is already registered on the local machine
// for the requested protocol. It just checks the one value required for this.
// See RegistryEntry::ExistsInRegistry for the behavior of |look_for_in|.
bool IsChromeRegisteredForProtocol(BrowserDistribution* dist,
                                   const base::string16& suffix,
                                   const base::string16& protocol,
                                   uint32 look_for_in) {
  ScopedVector<RegistryEntry> entries;
  RegistryEntry::GetProtocolCapabilityEntries(dist, suffix, protocol, &entries);
  return AreEntriesRegistered(entries, look_for_in);
}

// This method registers Chrome on Vista by launching an elevated setup.exe.
// That will show the user the standard Vista elevation prompt. If the user
// accepts it the new process will make the necessary changes and return SUCCESS
// that we capture and return.
// If protocol is non-empty we will also register Chrome as being capable of
// handling the protocol.
bool ElevateAndRegisterChrome(BrowserDistribution* dist,
                              const base::string16& chrome_exe,
                              const base::string16& suffix,
                              const base::string16& protocol) {
  // Only user-level installs prior to Windows 8 should need to elevate to
  // register.
  DCHECK(InstallUtil::IsPerUserInstall(chrome_exe.c_str()));
  DCHECK_LT(base::win::GetVersion(), base::win::VERSION_WIN8);
  base::FilePath exe_path =
      base::FilePath(chrome_exe).DirName().Append(installer::kSetupExe);
  if (!base::PathExists(exe_path)) {
    HKEY reg_root = InstallUtil::IsPerUserInstall(chrome_exe.c_str()) ?
        HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
    RegKey key(reg_root,
               dist->GetUninstallRegPath().c_str(),
               KEY_READ | KEY_WOW64_32KEY);
    base::string16 uninstall_string;
    key.ReadValue(installer::kUninstallStringField, &uninstall_string);
    CommandLine command_line = CommandLine::FromString(uninstall_string);
    exe_path = command_line.GetProgram();
  }

  if (base::PathExists(exe_path)) {
    CommandLine cmd(exe_path);
    cmd.AppendSwitchNative(installer::switches::kRegisterChromeBrowser,
                           chrome_exe);
    if (!suffix.empty()) {
      cmd.AppendSwitchNative(
          installer::switches::kRegisterChromeBrowserSuffix, suffix);
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
bool LaunchApplicationAssociationDialog(const base::string16& app_id) {
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
                             const base::string16& chrome_exe,
                             const base::string16& suffix,
                             RegistrationConfirmationLevel confirmation_level) {
  // Get the appropriate key to look for based on the level desired.
  base::string16 reg_key;
  switch (confirmation_level) {
    case CONFIRM_PROGID_REGISTRATION:
      // Software\Classes\ChromeHTML|suffix|
      reg_key = ShellUtil::kRegClasses;
      reg_key.push_back(base::FilePath::kSeparators[0]);
      reg_key.append(dist->GetBrowserProgIdPrefix());
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
    base::string16 hkcu_value;
    // If |reg_key| is present in HKCU, assert that it points to |chrome_exe|.
    // Otherwise, fall back on an HKLM lookup below.
    if (key_hkcu.ReadValue(L"", &hkcu_value) == ERROR_SUCCESS) {
      return InstallUtil::ProgramCompare(
          base::FilePath(chrome_exe)).Evaluate(hkcu_value);
    }
  }

  // Assert that |reg_key| points to |chrome_exe| in HKLM.
  const RegKey key_hklm(HKEY_LOCAL_MACHINE, reg_key.c_str(), KEY_QUERY_VALUE);
  base::string16 hklm_value;
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
                                   const base::string16& chrome_exe,
                                   base::string16* suffix) {
  if (!InstallUtil::IsPerUserInstall(chrome_exe.c_str()) ||
      QuickIsChromeRegistered(dist, chrome_exe, base::string16(),
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
                                    const base::string16& chrome_exe) {
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
bool RegisterChromeAsDefaultProtocolClientXPStyle(
    BrowserDistribution* dist,
    const base::string16& chrome_exe,
    const base::string16& protocol) {
  ScopedVector<RegistryEntry> entries;
  const base::string16 chrome_open(
      ShellUtil::GetChromeShellOpenCmd(chrome_exe));
  const base::string16 chrome_icon(
      ShellUtil::FormatIconLocation(
          chrome_exe,
          dist->GetIconIndex(BrowserDistribution::SHORTCUT_CHROME)));
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
// returns dist->GetShortcutName(BrowserDistribution::SHORTCUT_CHROME). In any
// case, it makes sure the return value is suffixed with ".lnk".
base::string16 ExtractShortcutNameFromProperties(
    BrowserDistribution* dist,
    const ShellUtil::ShortcutProperties& properties) {
  DCHECK(dist);
  base::string16 shortcut_name;
  if (properties.has_shortcut_name()) {
    shortcut_name = properties.shortcut_name;
  } else {
    shortcut_name =
        dist->GetShortcutName(BrowserDistribution::SHORTCUT_CHROME);
  }

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
void RemoveRunVerbOnWindows8(BrowserDistribution* dist,
                             const base::string16& chrome_exe) {
  if (IsChromeMetroSupported()) {
    bool is_per_user_install =InstallUtil::IsPerUserInstall(chrome_exe.c_str());
    HKEY root_key = DetermineRegistrationRoot(is_per_user_install);
    // There's no need to rollback, so forgo the usual work item lists and just
    // remove the key from the registry.
    base::string16 run_verb_key(ShellUtil::kRegClasses);
    run_verb_key.push_back(base::FilePath::kSeparators[0]);
    run_verb_key.append(ShellUtil::GetBrowserModelId(
        dist, is_per_user_install));
    run_verb_key.append(ShellUtil::kRegExePath);
    run_verb_key.append(ShellUtil::kRegShellPath);
    run_verb_key.push_back(base::FilePath::kSeparators[0]);
    run_verb_key.append(ShellUtil::kRegVerbRun);
    InstallUtil::DeleteRegistryKey(root_key, run_verb_key,
                                   WorkItem::kWow64Default);
  }
}

// Gets the short (8.3) form of |path|, putting the result in |short_path| and
// returning true on success.  |short_path| is not modified on failure.
bool ShortNameFromPath(const base::FilePath& path, base::string16* short_path) {
  DCHECK(short_path);
  base::string16 result(MAX_PATH, L'\0');
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
    const base::FilePath& chrome_exe,
    const wchar_t* const* protocols,
    size_t num_protocols) {
  base::win::ScopedComPtr<IApplicationAssociationRegistration> registration;
  HRESULT hr = registration.CreateInstance(
      CLSID_ApplicationAssociationRegistration, NULL, CLSCTX_INPROC);
  if (FAILED(hr))
    return ShellUtil::UNKNOWN_DEFAULT;

  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  base::string16 prog_id(dist->GetBrowserProgIdPrefix());
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
    const base::FilePath& chrome_exe,
    const wchar_t* const* protocols,
    size_t num_protocols) {
  base::win::ScopedComPtr<IApplicationAssociationRegistration> registration;
  HRESULT hr = registration.CreateInstance(
      CLSID_ApplicationAssociationRegistration, NULL, CLSCTX_INPROC);
  if (FAILED(hr))
    return ShellUtil::UNKNOWN_DEFAULT;

  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  base::string16 app_name(
      ShellUtil::GetApplicationName(dist, chrome_exe.value()));

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
    const base::FilePath& chrome_exe,
    const wchar_t* const* protocols,
    size_t num_protocols) {
  // Get its short (8.3) form.
  base::string16 short_app_path;
  if (!ShortNameFromPath(chrome_exe, &short_app_path))
    return ShellUtil::UNKNOWN_DEFAULT;

  const HKEY root_key = HKEY_CLASSES_ROOT;
  base::string16 key_path;
  base::win::RegKey key;
  base::string16 value;
  CommandLine command_line(CommandLine::NO_PROGRAM);
  base::string16 short_path;

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
    const base::FilePath& chrome_exe,
    const wchar_t* const* protocols,
    size_t num_protocols) {
#if DCHECK_IS_ON
  DCHECK(!num_protocols || protocols);
  for (size_t i = 0; i < num_protocols; ++i)
    DCHECK(protocols[i] && *protocols[i]);
#endif

  const base::win::Version windows_version = base::win::GetVersion();

  if (windows_version >= base::win::VERSION_WIN8)
    return ProbeCurrentDefaultHandlers(chrome_exe, protocols, num_protocols);
  else if (windows_version >= base::win::VERSION_VISTA)
    return ProbeAppIsDefaultHandlers(chrome_exe, protocols, num_protocols);

  return ProbeOpenCommandHandlers(chrome_exe, protocols, num_protocols);
}

// (Windows 8+) Finds and stores an app shortcuts folder path in *|path|.
// Returns true on success.
bool GetAppShortcutsFolder(BrowserDistribution* dist,
                           ShellUtil::ShellChange level,
                           base::FilePath *path) {
  DCHECK(path);
  DCHECK_GE(base::win::GetVersion(), base::win::VERSION_WIN8);

  base::FilePath folder;
  if (!PathService::Get(base::DIR_APP_SHORTCUTS, &folder)) {
    LOG(ERROR) << "Could not get application shortcuts location.";
    return false;
  }

  folder = folder.Append(
      ShellUtil::GetBrowserModelId(dist, level == ShellUtil::CURRENT_USER));
  if (!base::DirectoryExists(folder)) {
    VLOG(1) << "No start screen shortcuts.";
    return false;
  }

  *path = folder;
  return true;
}

// Shortcut filters for BatchShortcutAction().

typedef base::Callback<bool(const base::FilePath& /*shortcut_path*/,
                            const base::string16& /*args*/)>
    ShortcutFilterCallback;

// FilterTargetEq is a shortcut filter that matches only shortcuts that have a
// specific target, and optionally matches shortcuts that have non-empty
// arguments.
class FilterTargetEq {
 public:
  FilterTargetEq(const base::FilePath& desired_target_exe, bool require_args);

  // Returns true if filter rules are satisfied, i.e.:
  // - |target_path|'s target == |desired_target_compare_|, and
  // - |args| is non-empty (if |require_args_| == true).
  bool Match(const base::FilePath& target_path,
             const base::string16& args) const;

  // A convenience routine to create a callback to call Match().
  // The callback is only valid during the lifetime of the FilterTargetEq
  // instance.
  ShortcutFilterCallback AsShortcutFilterCallback();

 private:
  InstallUtil::ProgramCompare desired_target_compare_;

  bool require_args_;
};

FilterTargetEq::FilterTargetEq(const base::FilePath& desired_target_exe,
                               bool require_args)
    : desired_target_compare_(desired_target_exe),
      require_args_(require_args) {}

bool FilterTargetEq::Match(const base::FilePath& target_path,
                           const base::string16& args) const {
  if (!desired_target_compare_.EvaluatePath(target_path))
    return false;
  if (require_args_ && args.empty())
    return false;
  return true;
}

ShortcutFilterCallback FilterTargetEq::AsShortcutFilterCallback() {
  return base::Bind(&FilterTargetEq::Match, base::Unretained(this));
}

// Shortcut operations for BatchShortcutAction().

typedef base::Callback<bool(const base::FilePath& /*shortcut_path*/)>
    ShortcutOperationCallback;

bool ShortcutOpUnpin(const base::FilePath& shortcut_path) {
  VLOG(1) << "Trying to unpin " << shortcut_path.value();
  if (!base::win::TaskbarUnpinShortcutLink(shortcut_path.value().c_str())) {
    VLOG(1) << shortcut_path.value() << " wasn't pinned (or the unpin failed).";
    // No error, since shortcut might not be pinned.
  }
  return true;
}

bool ShortcutOpDelete(const base::FilePath& shortcut_path) {
  bool ret = base::DeleteFile(shortcut_path, false);
  LOG_IF(ERROR, !ret) << "Failed to remove " << shortcut_path.value();
  return ret;
}

bool ShortcutOpRetarget(const base::FilePath& old_target,
                        const base::FilePath& new_target,
                        const base::FilePath& shortcut_path) {
  base::win::ShortcutProperties new_prop;
  new_prop.set_target(new_target);

  // If the old icon matches old target, then update icon while keeping the old
  // icon index. Non-fatal if we fail to get the old icon.
  base::win::ShortcutProperties old_prop;
  if (base::win::ResolveShortcutProperties(
          shortcut_path,
          base::win::ShortcutProperties::PROPERTIES_ICON,
          &old_prop)) {
    if (InstallUtil::ProgramCompare(old_target).EvaluatePath(old_prop.icon))
      new_prop.set_icon(new_target, old_prop.icon_index);
  } else {
    LOG(ERROR) << "Failed to resolve " << shortcut_path.value();
  }

  bool result = base::win::CreateOrUpdateShortcutLink(
        shortcut_path, new_prop, base::win::SHORTCUT_UPDATE_EXISTING);
  LOG_IF(ERROR, !result) << "Failed to retarget " << shortcut_path.value();
  return result;
}

bool ShortcutOpListOrRemoveUnknownArgs(
    bool do_removal,
    std::vector<std::pair<base::FilePath, base::string16> >* shortcuts,
    const base::FilePath& shortcut_path) {
  base::string16 args;
  if (!base::win::ResolveShortcut(shortcut_path, NULL, &args))
    return false;

  CommandLine current_args(CommandLine::FromString(base::StringPrintf(
      L"unused_program %ls", args.c_str())));
  const char* const kept_switches[] = {
      switches::kApp,
      switches::kAppId,
      switches::kShowAppList,
      switches::kProfileDirectory,
  };
  CommandLine desired_args(CommandLine::NO_PROGRAM);
  desired_args.CopySwitchesFrom(current_args, kept_switches,
                                arraysize(kept_switches));
  if (desired_args.argv().size() == current_args.argv().size())
    return true;
  if (shortcuts)
    shortcuts->push_back(std::make_pair(shortcut_path, args));
  if (!do_removal)
    return true;
  base::win::ShortcutProperties updated_properties;
  updated_properties.set_arguments(desired_args.GetArgumentsString());
  return base::win::CreateOrUpdateShortcutLink(
      shortcut_path, updated_properties, base::win::SHORTCUT_UPDATE_EXISTING);
}

// {|location|, |dist|, |level|} determine |shortcut_folder|.
// For each shortcut in |shortcut_folder| that match |shortcut_filter|, apply
// |shortcut_operation|. Returns true if all operations are successful.
// All intended operations are attempted, even if failures occur.
// This method will abort and return false if |cancel| is non-NULL and gets set
// at any point during this call.
bool BatchShortcutAction(
    const ShortcutFilterCallback& shortcut_filter,
    const ShortcutOperationCallback& shortcut_operation,
    ShellUtil::ShortcutLocation location,
    BrowserDistribution* dist,
    ShellUtil::ShellChange level,
    const scoped_refptr<ShellUtil::SharedCancellationFlag>& cancel) {
  DCHECK(!shortcut_operation.is_null());
  base::FilePath shortcut_folder;
  if (!ShellUtil::GetShortcutPath(location, dist, level, &shortcut_folder)) {
    LOG(WARNING) << "Cannot find path at location " << location;
    return false;
  }

  bool success = true;
  base::FileEnumerator enumerator(
      shortcut_folder, false, base::FileEnumerator::FILES,
      base::string16(L"*") + installer::kLnkExt);
  base::FilePath target_path;
  base::string16 args;
  for (base::FilePath shortcut_path = enumerator.Next();
       !shortcut_path.empty();
       shortcut_path = enumerator.Next()) {
    if (cancel && cancel->data.IsSet())
      return false;
    if (base::win::ResolveShortcut(shortcut_path, &target_path, &args)) {
      if (shortcut_filter.Run(target_path, args) &&
          !shortcut_operation.Run(shortcut_path)) {
        success = false;
      }
    } else {
      LOG(ERROR) << "Cannot resolve shortcut at " << shortcut_path.value();
      success = false;
    }
  }
  return success;
}


// If the folder specified by {|location|, |dist|, |level|} is empty, remove it.
// Otherwise do nothing. Returns true on success, including the vacuous case
// where no deletion occurred because directory is non-empty.
bool RemoveShortcutFolderIfEmpty(ShellUtil::ShortcutLocation location,
                                 BrowserDistribution* dist,
                                 ShellUtil::ShellChange level) {
  // Explicitly whitelist locations, since accidental calls can be very harmful.
  if (location != ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR &&
      location != ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_APPS_DIR &&
      location != ShellUtil::SHORTCUT_LOCATION_APP_SHORTCUTS) {
    NOTREACHED();
    return false;
  }

  base::FilePath shortcut_folder;
  if (!ShellUtil::GetShortcutPath(location, dist, level, &shortcut_folder)) {
    LOG(WARNING) << "Cannot find path at location " << location;
    return false;
  }
  if (base::IsDirectoryEmpty(shortcut_folder) &&
      !base::DeleteFile(shortcut_folder, true)) {
    LOG(ERROR) << "Cannot remove folder " << shortcut_folder.value();
    return false;
  }
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

const wchar_t* ShellUtil::kDefaultFileAssociations[] = {L".htm", L".html",
    L".shtml", L".xht", L".xhtml", NULL};
const wchar_t* ShellUtil::kPotentialFileAssociations[] = {L".htm", L".html",
    L".shtml", L".xht", L".xhtml", L".webp", NULL};
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
                                              const base::string16& chrome_exe,
                                              const base::string16& suffix) {
  return QuickIsChromeRegistered(dist, chrome_exe, suffix,
                                 CONFIRM_SHELL_REGISTRATION_IN_HKLM);
}

bool ShellUtil::ShortcutLocationIsSupported(
    ShellUtil::ShortcutLocation location) {
  switch (location) {
    case SHORTCUT_LOCATION_DESKTOP:  // Falls through.
    case SHORTCUT_LOCATION_QUICK_LAUNCH:  // Falls through.
    case SHORTCUT_LOCATION_START_MENU_ROOT:  // Falls through.
    case SHORTCUT_LOCATION_START_MENU_CHROME_DIR:  // Falls through.
    case SHORTCUT_LOCATION_START_MENU_CHROME_APPS_DIR:
      return true;
    case SHORTCUT_LOCATION_TASKBAR_PINS:
      return base::win::GetVersion() >= base::win::VERSION_WIN7;
    case SHORTCUT_LOCATION_APP_SHORTCUTS:
      return base::win::GetVersion() >= base::win::VERSION_WIN8;
    default:
      NOTREACHED();
      return false;
  }
}

bool ShellUtil::GetShortcutPath(ShellUtil::ShortcutLocation location,
                                BrowserDistribution* dist,
                                ShellChange level,
                                base::FilePath* path) {
  DCHECK(path);
  int dir_key = -1;
  base::string16 folder_to_append;
  switch (location) {
    case SHORTCUT_LOCATION_DESKTOP:
      dir_key = (level == CURRENT_USER) ? base::DIR_USER_DESKTOP :
                                          base::DIR_COMMON_DESKTOP;
      break;
    case SHORTCUT_LOCATION_QUICK_LAUNCH:
      dir_key = (level == CURRENT_USER) ? base::DIR_USER_QUICK_LAUNCH :
                                          base::DIR_DEFAULT_USER_QUICK_LAUNCH;
      break;
    case SHORTCUT_LOCATION_START_MENU_ROOT:
      dir_key = (level == CURRENT_USER) ? base::DIR_START_MENU :
                                          base::DIR_COMMON_START_MENU;
      break;
    case SHORTCUT_LOCATION_START_MENU_CHROME_DIR:
      dir_key = (level == CURRENT_USER) ? base::DIR_START_MENU :
                                          base::DIR_COMMON_START_MENU;
      folder_to_append = dist->GetStartMenuShortcutSubfolder(
          BrowserDistribution::SUBFOLDER_CHROME);
      break;
    case SHORTCUT_LOCATION_START_MENU_CHROME_APPS_DIR:
      dir_key = (level == CURRENT_USER) ? base::DIR_START_MENU :
                                          base::DIR_COMMON_START_MENU;
      folder_to_append = dist->GetStartMenuShortcutSubfolder(
          BrowserDistribution::SUBFOLDER_APPS);
      break;
    case SHORTCUT_LOCATION_TASKBAR_PINS:
      dir_key = base::DIR_TASKBAR_PINS;
      break;
    case SHORTCUT_LOCATION_APP_SHORTCUTS:
      // TODO(huangs): Move GetAppShortcutsFolder() logic into base_paths_win.
      return GetAppShortcutsFolder(dist, level, path);

    default:
      NOTREACHED();
      return false;
  }

  if (!PathService::Get(dir_key, path) || path->empty()) {
    NOTREACHED() << dir_key;
    return false;
  }

  if (!folder_to_append.empty())
    *path = path->Append(folder_to_append);

  return true;
}

bool ShellUtil::CreateOrUpdateShortcut(
    ShellUtil::ShortcutLocation location,
    BrowserDistribution* dist,
    const ShellUtil::ShortcutProperties& properties,
    ShellUtil::ShortcutOperation operation) {
  // Explicitly whitelist locations to which this is applicable.
  if (location != SHORTCUT_LOCATION_DESKTOP &&
      location != SHORTCUT_LOCATION_QUICK_LAUNCH &&
      location != SHORTCUT_LOCATION_START_MENU_ROOT &&
      location != SHORTCUT_LOCATION_START_MENU_CHROME_DIR &&
      location != SHORTCUT_LOCATION_START_MENU_CHROME_APPS_DIR) {
    NOTREACHED();
    return false;
  }

  DCHECK(dist);
  // |pin_to_taskbar| is only acknowledged when first creating the shortcut.
  DCHECK(!properties.pin_to_taskbar ||
         operation == SHELL_SHORTCUT_CREATE_ALWAYS ||
         operation == SHELL_SHORTCUT_CREATE_IF_NO_SYSTEM_LEVEL);

  base::FilePath user_shortcut_path;
  base::FilePath system_shortcut_path;
  if (!GetShortcutPath(location, dist, SYSTEM_LEVEL, &system_shortcut_path)) {
    NOTREACHED();
    return false;
  }

  base::string16 shortcut_name(
      ExtractShortcutNameFromProperties(dist, properties));
  system_shortcut_path = system_shortcut_path.Append(shortcut_name);

  base::FilePath* chosen_path;
  bool should_install_shortcut = true;
  if (properties.level == SYSTEM_LEVEL) {
    // Install the system-level shortcut if requested.
    chosen_path = &system_shortcut_path;
  } else if (operation != SHELL_SHORTCUT_CREATE_IF_NO_SYSTEM_LEVEL ||
             !base::PathExists(system_shortcut_path)) {
    // Otherwise install the user-level shortcut, unless the system-level
    // variant of this shortcut is present on the machine and |operation| states
    // not to create a user-level shortcut in that case.
    if (!GetShortcutPath(location, dist, CURRENT_USER, &user_shortcut_path)) {
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
        !base::CreateDirectory(chosen_path->DirName())) {
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

base::string16 ShellUtil::FormatIconLocation(const base::string16& icon_path,
                                             int icon_index) {
  base::string16 icon_string(icon_path);
  icon_string.append(L",");
  icon_string.append(base::IntToString16(icon_index));
  return icon_string;
}

base::string16 ShellUtil::GetChromeShellOpenCmd(
    const base::string16& chrome_exe) {
  return L"\"" + chrome_exe + L"\" -- \"%1\"";
}

base::string16 ShellUtil::GetChromeDelegateCommand(
    const base::string16& chrome_exe) {
  return L"\"" + chrome_exe + L"\" -- %*";
}

void ShellUtil::GetRegisteredBrowsers(
    BrowserDistribution* dist,
    std::map<base::string16, base::string16>* browsers) {
  DCHECK(dist);
  DCHECK(browsers);

  const base::string16 base_key(ShellUtil::kRegStartMenuInternet);
  base::string16 client_path;
  RegKey key;
  base::string16 name;
  base::string16 command;

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
          name.find(dist->GetBaseAppName()) != base::string16::npos) {
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

base::string16 ShellUtil::GetCurrentInstallationSuffix(
    BrowserDistribution* dist,
    const base::string16& chrome_exe) {
  // This method is somewhat the opposite of GetInstallationSpecificSuffix().
  // In this case we are not trying to determine the current suffix for the
  // upcoming installation (i.e. not trying to stick to a currently bad
  // registration style if one is present).
  // Here we want to determine which suffix we should use at run-time.
  // In order of preference, we prefer (for user-level installs):
  //   1) Base 32 encoding of the md5 hash of the user's sid (new-style).
  //   2) Username (old-style).
  //   3) Unsuffixed (even worse).
  base::string16 tested_suffix;
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

base::string16 ShellUtil::GetApplicationName(BrowserDistribution* dist,
                                             const base::string16& chrome_exe) {
  base::string16 app_name = dist->GetBaseAppName();
  app_name += GetCurrentInstallationSuffix(dist, chrome_exe);
  return app_name;
}

base::string16 ShellUtil::GetBrowserModelId(BrowserDistribution* dist,
                                            bool is_per_user_install) {
  base::string16 app_id(dist->GetBaseAppId());
  base::string16 suffix;

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
  std::vector<base::string16> components(1, app_id.append(suffix));
  return BuildAppModelId(components);
}

base::string16 ShellUtil::BuildAppModelId(
    const std::vector<base::string16>& components) {
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

  base::string16 app_id;
  app_id.reserve(installer::kMaxAppModelIdLength);
  for (std::vector<base::string16>::const_iterator it = components.begin();
       it != components.end(); ++it) {
    if (it != components.begin())
      app_id.push_back(L'.');

    const base::string16& component = *it;
    DCHECK(!component.empty());
    if (component.length() > max_component_length) {
      // Append a shortened version of this component. Cut in the middle to try
      // to avoid losing the unique parts of this component (which are usually
      // at the beginning or end for things like usernames and paths).
      app_id.append(component.c_str(), 0, max_component_length / 2);
      app_id.append(component.c_str(),
                    component.length() - ((max_component_length + 1) / 2),
                    base::string16::npos);
    } else {
      app_id.append(component);
    }
  }
  // No spaces are allowed in the AppUserModelId according to MSDN.
  base::ReplaceChars(app_id, base::ASCIIToUTF16(" "), base::ASCIIToUTF16("_"),
                     &app_id);
  return app_id;
}

ShellUtil::DefaultState ShellUtil::GetChromeDefaultState() {
  base::FilePath app_path;
  if (!PathService::Get(base::FILE_EXE, &app_path)) {
    NOTREACHED();
    return ShellUtil::UNKNOWN_DEFAULT;
  }

  return GetChromeDefaultStateFromPath(app_path);
}

ShellUtil::DefaultState ShellUtil::GetChromeDefaultStateFromPath(
    const base::FilePath& chrome_exe) {
  BrowserDistribution* distribution = BrowserDistribution::GetDistribution();
  if (distribution->GetDefaultBrowserControlPolicy() ==
      BrowserDistribution::DEFAULT_BROWSER_UNSUPPORTED) {
    return NOT_DEFAULT;
  }
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
  return ProbeProtocolHandlers(chrome_exe,
                               kChromeProtocols,
                               arraysize(kChromeProtocols));
}

ShellUtil::DefaultState ShellUtil::GetChromeDefaultProtocolClientState(
    const base::string16& protocol) {
  BrowserDistribution* distribution = BrowserDistribution::GetDistribution();
  if (distribution->GetDefaultBrowserControlPolicy() ==
      BrowserDistribution::DEFAULT_BROWSER_UNSUPPORTED) {
    return NOT_DEFAULT;
  }

  if (protocol.empty())
    return UNKNOWN_DEFAULT;

  base::FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED();
    return ShellUtil::UNKNOWN_DEFAULT;
  }

  const wchar_t* const protocols[] = { protocol.c_str() };
  return ProbeProtocolHandlers(chrome_exe,
                               protocols,
                               arraysize(protocols));
}

// static
bool ShellUtil::CanMakeChromeDefaultUnattended() {
  return base::win::GetVersion() < base::win::VERSION_WIN8;
}

bool ShellUtil::MakeChromeDefault(BrowserDistribution* dist,
                                  int shell_change,
                                  const base::string16& chrome_exe,
                                  bool elevate_if_not_admin) {
  DCHECK(!(shell_change & ShellUtil::SYSTEM_LEVEL) || IsUserAnAdmin());

  BrowserDistribution* distribution = BrowserDistribution::GetDistribution();
  if (distribution->GetDefaultBrowserControlPolicy() !=
      BrowserDistribution::DEFAULT_BROWSER_FULL_CONTROL) {
    return false;
  }

  // Windows 8 does not permit making a browser default just like that.
  // This process needs to be routed through the system's UI. Use
  // ShowMakeChromeDefaultSystemUI instead (below).
  if (!CanMakeChromeDefaultUnattended()) {
    return false;
  }

  if (!ShellUtil::RegisterChromeBrowser(
           dist, chrome_exe, base::string16(), elevate_if_not_admin)) {
    return false;
  }

  bool ret = true;
  // First use the new "recommended" way on Vista to make Chrome default
  // browser.
  base::string16 app_name = GetApplicationName(dist, chrome_exe);

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

      for (int i = 0; ShellUtil::kDefaultFileAssociations[i] != NULL; i++) {
        hr = pAAR->SetAppAsDefault(app_name.c_str(),
            ShellUtil::kDefaultFileAssociations[i], AT_FILEEXTENSION);
        if (!SUCCEEDED(hr)) {
          ret = false;
          LOG(ERROR) << "Failed to register as default for file extension "
                     << ShellUtil::kDefaultFileAssociations[i]
                     << " (" << hr << ")";
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

bool ShellUtil::ShowMakeChromeDefaultSystemUI(
    BrowserDistribution* dist,
    const base::string16& chrome_exe) {
  DCHECK_GE(base::win::GetVersion(), base::win::VERSION_WIN8);
  if (dist->GetDefaultBrowserControlPolicy() !=
      BrowserDistribution::DEFAULT_BROWSER_FULL_CONTROL) {
    return false;
  }

  if (!RegisterChromeBrowser(dist, chrome_exe, base::string16(), true))
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

bool ShellUtil::MakeChromeDefaultProtocolClient(
    BrowserDistribution* dist,
    const base::string16& chrome_exe,
    const base::string16& protocol) {
  if (dist->GetDefaultBrowserControlPolicy() !=
      BrowserDistribution::DEFAULT_BROWSER_FULL_CONTROL) {
    return false;
  }

  if (!RegisterChromeForProtocol(
           dist, chrome_exe, base::string16(), protocol, true))
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
      base::string16 app_name = GetApplicationName(dist, chrome_exe);
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
    const base::string16& chrome_exe,
    const base::string16& protocol) {
  DCHECK_GE(base::win::GetVersion(), base::win::VERSION_WIN8);
  if (dist->GetDefaultBrowserControlPolicy() !=
      BrowserDistribution::DEFAULT_BROWSER_FULL_CONTROL) {
    return false;
  }

  if (!RegisterChromeForProtocol(
           dist, chrome_exe, base::string16(), protocol, true))
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
                                      const base::string16& chrome_exe,
                                      const base::string16& unique_suffix,
                                      bool elevate_if_not_admin) {
  if (dist->GetDefaultBrowserControlPolicy() ==
      BrowserDistribution::DEFAULT_BROWSER_UNSUPPORTED) {
    return false;
  }

  CommandLine& command_line = *CommandLine::ForCurrentProcess();

  base::string16 suffix;
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

  bool user_level = InstallUtil::IsPerUserInstall(chrome_exe.c_str());
  HKEY root = DetermineRegistrationRoot(user_level);

  // Look only in HKLM for system-level installs (otherwise, if a user-level
  // install is also present, it will lead IsChromeRegistered() to think this
  // system-level install isn't registered properly as it is shadowed by the
  // user-level install's registrations).
  uint32 look_for_in = user_level ?
      RegistryEntry::LOOK_IN_HKCU_THEN_HKLM : RegistryEntry::LOOK_IN_HKLM;

  // Check if chrome is already registered with this suffix.
  if (IsChromeRegistered(dist, chrome_exe, suffix, look_for_in))
    return true;

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
      ElevateAndRegisterChrome(dist, chrome_exe, suffix, base::string16())) {
    // If the user is not an admin and OS is between Vista and Windows 7
    // inclusively, try to elevate and register. This is only intended for
    // user-level installs as system-level installs should always be run with
    // admin rights.
    result = true;
  } else {
    // If we got to this point then all we can do is create ProgId and basic app
    // registrations under HKCU.
    ScopedVector<RegistryEntry> entries;
    RegistryEntry::GetProgIdEntries(
        dist, chrome_exe, base::string16(), &entries);
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
      RegistryEntry::GetAppRegistrationEntries(chrome_exe, base::string16(),
                                               &entries);
      result = AddRegistryEntries(HKEY_CURRENT_USER, entries);
    }
  }
  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
  return result;
}

bool ShellUtil::RegisterChromeForProtocol(BrowserDistribution* dist,
                                          const base::string16& chrome_exe,
                                          const base::string16& unique_suffix,
                                          const base::string16& protocol,
                                          bool elevate_if_not_admin) {
  if (dist->GetDefaultBrowserControlPolicy() ==
      BrowserDistribution::DEFAULT_BROWSER_UNSUPPORTED) {
    return false;
  }

  base::string16 suffix;
  if (!unique_suffix.empty()) {
    suffix = unique_suffix;
  } else if (!GetInstallationSpecificSuffix(dist, chrome_exe, &suffix)) {
    return false;
  }

  bool user_level = InstallUtil::IsPerUserInstall(chrome_exe.c_str());
  HKEY root = DetermineRegistrationRoot(user_level);

  // Look only in HKLM for system-level installs (otherwise, if a user-level
  // install is also present, it could lead IsChromeRegisteredForProtocol() to
  // think this system-level install isn't registered properly as it may be
  // shadowed by the user-level install's registrations).
  uint32 look_for_in = user_level ?
      RegistryEntry::LOOK_IN_HKCU_THEN_HKLM : RegistryEntry::LOOK_IN_HKLM;

  // Check if chrome is already registered with this suffix.
  if (IsChromeRegisteredForProtocol(dist, suffix, protocol, look_for_in))
    return true;

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

// static
bool ShellUtil::RemoveShortcuts(ShellUtil::ShortcutLocation location,
                                BrowserDistribution* dist,
                                ShellChange level,
                                const base::FilePath& target_exe) {
  if (!ShellUtil::ShortcutLocationIsSupported(location))
    return true;  // Vacuous success.

  FilterTargetEq shortcut_filter(target_exe, false);
  // Main operation to apply to each shortcut in the directory specified.
  ShortcutOperationCallback shortcut_operation(
      location == SHORTCUT_LOCATION_TASKBAR_PINS ?
          base::Bind(&ShortcutOpUnpin) : base::Bind(&ShortcutOpDelete));
  bool success = BatchShortcutAction(shortcut_filter.AsShortcutFilterCallback(),
                                     shortcut_operation, location, dist, level,
                                     NULL);
  // Remove chrome-specific shortcut folders if they are now empty.
  if (success &&
      (location == SHORTCUT_LOCATION_START_MENU_CHROME_DIR ||
       location == SHORTCUT_LOCATION_START_MENU_CHROME_APPS_DIR ||
       location == SHORTCUT_LOCATION_APP_SHORTCUTS)) {
    success = RemoveShortcutFolderIfEmpty(location, dist, level);
  }
  return success;
}

// static
bool ShellUtil::RetargetShortcutsWithArgs(
    ShellUtil::ShortcutLocation location,
    BrowserDistribution* dist,
    ShellChange level,
    const base::FilePath& old_target_exe,
    const base::FilePath& new_target_exe) {
  if (!ShellUtil::ShortcutLocationIsSupported(location))
    return true;  // Vacuous success.

  FilterTargetEq shortcut_filter(old_target_exe, true);
  ShortcutOperationCallback shortcut_operation(
      base::Bind(&ShortcutOpRetarget, old_target_exe, new_target_exe));
  return BatchShortcutAction(shortcut_filter.AsShortcutFilterCallback(),
                             shortcut_operation, location, dist, level, NULL);
}

// static
bool ShellUtil::ShortcutListMaybeRemoveUnknownArgs(
    ShellUtil::ShortcutLocation location,
    BrowserDistribution* dist,
    ShellChange level,
    const base::FilePath& chrome_exe,
    bool do_removal,
    const scoped_refptr<SharedCancellationFlag>& cancel,
    std::vector<std::pair<base::FilePath, base::string16> >* shortcuts) {
  if (!ShellUtil::ShortcutLocationIsSupported(location))
    return false;
  DCHECK(dist);
  FilterTargetEq shortcut_filter(chrome_exe, true);
  ShortcutOperationCallback shortcut_operation(
      base::Bind(&ShortcutOpListOrRemoveUnknownArgs, do_removal, shortcuts));
  return BatchShortcutAction(shortcut_filter.AsShortcutFilterCallback(),
                             shortcut_operation, location, dist, level, cancel);
}

bool ShellUtil::GetUserSpecificRegistrySuffix(base::string16* suffix) {
  // Use a thread-safe cache for the user's suffix.
  static base::LazyInstance<UserSpecificRegistrySuffix>::Leaky suffix_instance =
      LAZY_INSTANCE_INITIALIZER;
  return suffix_instance.Get().GetSuffix(suffix);
}

bool ShellUtil::GetOldUserSpecificRegistrySuffix(base::string16* suffix) {
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

base::string16 ShellUtil::ByteArrayToBase32(const uint8* bytes, size_t size) {
  static const char kEncoding[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

  // Eliminate special cases first.
  if (size == 0) {
    return base::string16();
  } else if (size == 1) {
    base::string16 ret;
    ret.push_back(kEncoding[(bytes[0] & 0xf8) >> 3]);
    ret.push_back(kEncoding[(bytes[0] & 0x07) << 2]);
    return ret;
  } else if (size >= std::numeric_limits<size_t>::max() / 8) {
    // If |size| is too big, the calculation of |encoded_length| below will
    // overflow.
    NOTREACHED();
    return base::string16();
  }

  // Overestimate the number of bits in the string by 4 so that dividing by 5
  // is the equivalent of rounding up the actual number of bits divided by 5.
  const size_t encoded_length = (size * 8 + 4) / 5;

  base::string16 ret;
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
