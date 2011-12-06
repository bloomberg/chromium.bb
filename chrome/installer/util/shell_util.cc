// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "base/win/scoped_comptr.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"

#include "installer_util_strings.h"

using base::win::RegKey;

namespace {

const wchar_t kReinstallCommand[] = L"ReinstallCommand";

// This class represents a single registry entry. The objective is to
// encapsulate all the registry entries required for registering Chrome at one
// place. This class can not be instantiated outside the class and the objects
// of this class type can be obtained only by calling a static method of this
// class.
class RegistryEntry {
 public:
  // Returns the Windows browser client registration key for Chrome.  For
  // example: "Software\Clients\StartMenuInternet\Chromium[.user]".  Strictly
  // speaking, we should use the name of the executable (e.g., "chrome.exe"),
  // but that ship has sailed.  The cost of switching now is re-prompting users
  // to make Chrome their default browser, which isn't polite.  |suffix| is the
  // user-specific registration suffix; see GetUserSpecificDefaultBrowserSuffix
  // in shell_util.h for details.
  static std::wstring GetBrowserClientKey(BrowserDistribution* dist,
                                          const std::wstring& suffix) {
    DCHECK(suffix.empty() || suffix[0] == L'.');
    return std::wstring(ShellUtil::kRegStartMenuInternet)
        .append(1, L'\\')
        .append(dist->GetApplicationName())
        .append(suffix);
  }

  // Returns the Windows Default Programs capabilities key for Chrome.  For
  // example:
  // "Software\Clients\StartMenuInternet\Chromium[.user]\Capabilities".
  static std::wstring GetCapabilitiesKey(BrowserDistribution* dist,
                                         const std::wstring& suffix) {
    return GetBrowserClientKey(dist, suffix).append(L"\\Capabilities");
  }

  // This method returns a list of all the registry entries that
  // are needed to register Chromium ProgIds.
  static bool GetProgIdEntries(BrowserDistribution* dist,
                               const std::wstring& chrome_exe,
                               const std::wstring& suffix,
                               std::list<RegistryEntry*>* entries) {
    std::wstring icon_path = ShellUtil::GetChromeIcon(dist, chrome_exe);
    std::wstring open_cmd = ShellUtil::GetChromeShellOpenCmd(chrome_exe);

    // File association ProgId
    std::wstring chrome_html_prog_id(ShellUtil::kRegClasses);
    file_util::AppendToPath(&chrome_html_prog_id, ShellUtil::kChromeHTMLProgId);
    chrome_html_prog_id.append(suffix);
    entries->push_front(new RegistryEntry(
        chrome_html_prog_id, ShellUtil::kChromeHTMLProgIdDesc));
    entries->push_front(new RegistryEntry(
        chrome_html_prog_id, ShellUtil::kRegUrlProtocol, L""));
    entries->push_front(new RegistryEntry(
        chrome_html_prog_id + ShellUtil::kRegDefaultIcon, icon_path));
    entries->push_front(new RegistryEntry(
        chrome_html_prog_id + ShellUtil::kRegShellOpen, open_cmd));

    return true;
  }

  // This method returns a list of the system level registry entries
  // needed to declare a capability of handling a protocol.
  static bool GetProtocolCapabilityEntries(BrowserDistribution* dist,
                                           const std::wstring& suffix,
                                           const std::wstring& protocol,
                                           std::list<RegistryEntry*>* entries) {
    entries->push_front(new RegistryEntry(
        GetCapabilitiesKey(dist, suffix).append(L"\\URLAssociations"),
        protocol, std::wstring(ShellUtil::kChromeHTMLProgId).append(suffix)));
    return true;
  }

  // This method returns a list of all the system level registry entries that
  // are needed to register Chromium on the machine.
  static bool GetSystemEntries(BrowserDistribution* dist,
                               const std::wstring& chrome_exe,
                               const std::wstring& suffix,
                               std::list<RegistryEntry*>* entries) {
    std::wstring icon_path = ShellUtil::GetChromeIcon(dist, chrome_exe);
    std::wstring quoted_exe_path = L"\"" + chrome_exe + L"\"";

    // Register for the Start Menu "Internet" link (pre-Win7).
    const std::wstring start_menu_entry(GetBrowserClientKey(dist, suffix));
    // Register Chrome's display name.
    // TODO(grt): http://crbug.com/75152 Also set LocalizedString; see
    // http://msdn.microsoft.com/en-us/library/windows/desktop/cc144109(v=VS.85).aspx#registering_the_display_name
    entries->push_front(new RegistryEntry(
        start_menu_entry, dist->GetApplicationName()));
    // Register the "open" verb for launching Chrome via the "Internet" link.
    entries->push_front(new RegistryEntry(
        start_menu_entry + ShellUtil::kRegShellOpen, quoted_exe_path));
    // Register Chrome's icon for the Start Menu "Internet" link.
    entries->push_front(new RegistryEntry(
        start_menu_entry + ShellUtil::kRegDefaultIcon, icon_path));

    // Register installation information.
    std::wstring install_info(start_menu_entry + L"\\InstallInfo");
    // Note: not using CommandLine since it has ambiguous rules for quoting
    // strings.
    entries->push_front(new RegistryEntry(install_info, kReinstallCommand,
        quoted_exe_path + L" --" + ASCIIToWide(switches::kMakeDefaultBrowser)));
    entries->push_front(new RegistryEntry(install_info, L"HideIconsCommand",
        quoted_exe_path + L" --" + ASCIIToWide(switches::kHideIcons)));
    entries->push_front(new RegistryEntry(install_info, L"ShowIconsCommand",
        quoted_exe_path + L" --" + ASCIIToWide(switches::kShowIcons)));
    entries->push_front(new RegistryEntry(install_info, L"IconsVisible", 1));

    // Register with Default Programs.
    std::wstring app_name(dist->GetApplicationName().append(suffix));
    // Tell Windows where to find Chrome's Default Programs info.
    std::wstring capabilities(GetCapabilitiesKey(dist, suffix));
    entries->push_front(new RegistryEntry(ShellUtil::kRegRegisteredApplications,
        app_name, capabilities));
    // Write out Chrome's Default Programs info.
    // TODO(grt): http://crbug.com/75152 Write a reference to a localized
    // resource rather than this.
    entries->push_front(new RegistryEntry(
        capabilities, L"ApplicationDescription",
        dist->GetLongAppDescription()));
    entries->push_front(new RegistryEntry(
        capabilities, L"ApplicationIcon", icon_path));
    entries->push_front(new RegistryEntry(
        capabilities, L"ApplicationName", app_name));

    entries->push_front(new RegistryEntry(capabilities + L"\\Startmenu",
        L"StartMenuInternet", app_name));

    std::wstring html_prog_id(ShellUtil::kChromeHTMLProgId);
    html_prog_id.append(suffix);
    for (int i = 0; ShellUtil::kFileAssociations[i] != NULL; i++) {
      entries->push_front(new RegistryEntry(
          capabilities + L"\\FileAssociations",
          ShellUtil::kFileAssociations[i], html_prog_id));
    }
    for (int i = 0; ShellUtil::kPotentialProtocolAssociations[i] != NULL;
        i++) {
      entries->push_front(new RegistryEntry(
          capabilities + L"\\URLAssociations",
          ShellUtil::kPotentialProtocolAssociations[i], html_prog_id));
    }

    // Application Registration.
    FilePath chrome_path(chrome_exe);
    std::wstring app_path_key(ShellUtil::kAppPathsRegistryKey);
    file_util::AppendToPath(&app_path_key, chrome_path.BaseName().value());
    entries->push_front(new RegistryEntry(app_path_key, chrome_exe));
    entries->push_front(new RegistryEntry(app_path_key,
        ShellUtil::kAppPathsRegistryPathName, chrome_path.DirName().value()));

    // TODO: add chrome to open with list (Bug 16726).
    return true;
  }

  // This method returns a list of all the user level registry entries that
  // are needed to make Chromium the default handler for a protocol.
  static bool GetUserProtocolEntries(const std::wstring& protocol,
                                     const std::wstring& chrome_icon,
                                     const std::wstring& chrome_open,
                                     std::list<RegistryEntry*>* entries) {
    // Protocols associations.
    std::wstring url_key(ShellUtil::kRegClasses);
    file_util::AppendToPath(&url_key, protocol);

    // This registry value tells Windows that this 'class' is a URL scheme
    // so IE, explorer and other apps will route it to our handler.
    // <root hkey>\Software\Classes\<protocol>\URL Protocol
    entries->push_front(new RegistryEntry(url_key,
        ShellUtil::kRegUrlProtocol, L""));

    // <root hkey>\Software\Classes\<protocol>\DefaultIcon
    std::wstring icon_key = url_key + ShellUtil::kRegDefaultIcon;
    entries->push_front(new RegistryEntry(icon_key, chrome_icon));

    // <root hkey>\Software\Classes\<protocol>\shell\open\command
    std::wstring shell_key = url_key + ShellUtil::kRegShellOpen;
    entries->push_front(new RegistryEntry(shell_key, chrome_open));

    // <root hkey>\Software\Classes\<protocol>\shell\open\ddeexec
    std::wstring dde_key = url_key + L"\\shell\\open\\ddeexec";
    entries->push_front(new RegistryEntry(dde_key, L""));

    // <root hkey>\Software\Classes\<protocol>\shell\@
    std::wstring protocol_shell_key = url_key + ShellUtil::kRegShellPath;
    entries->push_front(new RegistryEntry(protocol_shell_key, L"open"));

    return true;
  }

  // This method returns a list of all the user level registry entries that
  // are needed to make Chromium default browser.
  static bool GetUserEntries(BrowserDistribution* dist,
                             const std::wstring& chrome_exe,
                             const std::wstring& suffix,
                             std::list<RegistryEntry*>* entries) {
    // File extension associations.
    std::wstring html_prog_id(ShellUtil::kChromeHTMLProgId);
    html_prog_id.append(suffix);
    for (int i = 0; ShellUtil::kFileAssociations[i] != NULL; i++) {
      std::wstring ext_key(ShellUtil::kRegClasses);
      file_util::AppendToPath(&ext_key, ShellUtil::kFileAssociations[i]);
      entries->push_front(new RegistryEntry(ext_key, html_prog_id));
    }

    // Protocols associations.
    std::wstring chrome_open = ShellUtil::GetChromeShellOpenCmd(chrome_exe);
    std::wstring chrome_icon = ShellUtil::GetChromeIcon(dist, chrome_exe);
    for (int i = 0; ShellUtil::kBrowserProtocolAssociations[i] != NULL; i++) {
      GetUserProtocolEntries(ShellUtil::kBrowserProtocolAssociations[i],
                             chrome_icon, chrome_open, entries);
    }

    // start->Internet shortcut.
    std::wstring start_menu(ShellUtil::kRegStartMenuInternet);
    std::wstring app_name = dist->GetApplicationName() + suffix;
    entries->push_front(new RegistryEntry(start_menu, app_name));
    return true;
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

  // Checks if the current registry entry exists in HKLM registry and the value
  // is same.
  bool ExistsInHKLM() const {
    RegKey key(HKEY_LOCAL_MACHINE, _key_path.c_str(), KEY_READ);
    bool found = false;
    if (_is_string) {
      std::wstring read_value;
      found = (key.ReadValue(_name.c_str(), &read_value) == ERROR_SUCCESS) &&
              (read_value.size() == _value.size()) &&
              (std::equal(_value.begin(), _value.end(), read_value.begin(),
                          base::CaseInsensitiveCompare<wchar_t>()));
    } else {
      DWORD read_value;
      found = (key.ReadValueDW(_name.c_str(), &read_value) == ERROR_SUCCESS) &&
              (read_value == _int_value);
    }
    key.Close();
    return found;
  }

  // Checks if the current registry entry exists in HKLM registry
  // (only the name).
  bool NameExistsInHKLM() const {
    RegKey key(HKEY_LOCAL_MACHINE, _key_path.c_str(), KEY_READ);
    bool found = false;
    if (_is_string) {
      std::wstring read_value;
      found = key.ReadValue(_name.c_str(), &read_value) == ERROR_SUCCESS;
    } else {
      DWORD read_value;
      found = key.ReadValueDW(_name.c_str(), &read_value) == ERROR_SUCCESS;
    }
    key.Close();
    return found;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RegistryEntry);

  // Create a object that represent default value of a key
  RegistryEntry(const std::wstring& key_path, const std::wstring& value)
      : _key_path(key_path), _name(),
        _is_string(true), _value(value), _int_value(0) {
  }

  // Create a object that represent a key of type REG_SZ
  RegistryEntry(const std::wstring& key_path, const std::wstring& name,
                const std::wstring& value)
      : _key_path(key_path), _name(name),
        _is_string(true), _value(value), _int_value(0) {
  }

  // Create a object that represent a key of integer type
  RegistryEntry(const std::wstring& key_path, const std::wstring& name,
                DWORD value)
      : _key_path(key_path), _name(name),
        _is_string(false), _value(), _int_value(value) {
  }

  std::wstring _key_path;  // key path for the registry entry
  std::wstring _name;      // name of the registry entry
  bool _is_string;         // true if current registry entry is of type REG_SZ
  std::wstring _value;     // string value (useful if _is_string = true)
  DWORD _int_value;        // integer value (useful if _is_string = false)
};  // class RegistryEntry


// This method converts all the RegistryEntries from the given list to
// Set/CreateRegWorkItems and runs them using WorkItemList.
bool AddRegistryEntries(HKEY root, const std::list<RegistryEntry*>& entries) {
  scoped_ptr<WorkItemList> items(WorkItem::CreateWorkItemList());

  for (std::list<RegistryEntry*>::const_iterator itr = entries.begin();
       itr != entries.end(); ++itr)
    (*itr)->AddToWorkItemList(root, items.get());

  // Apply all the registry changes and if there is a problem, rollback
  if (!items->Do()) {
    items->Rollback();
    return false;
  }
  return true;
}

// This method checks if Chrome is already registered on the local machine.
// It gets all the required registry entries for Chrome and then checks if
// they exist in HKLM. Returns true if all the entries exist, otherwise false.
bool IsChromeRegistered(BrowserDistribution* dist,
                        const std::wstring& chrome_exe,
                        const std::wstring& suffix) {
  bool registered = true;
  std::list<RegistryEntry*> entries;
  STLElementDeleter<std::list<RegistryEntry*>> entries_deleter(&entries);
  RegistryEntry::GetProgIdEntries(dist, chrome_exe, suffix, &entries);
  RegistryEntry::GetSystemEntries(dist, chrome_exe, suffix, &entries);
  for (std::list<RegistryEntry*>::const_iterator itr = entries.begin();
       itr != entries.end() && registered; ++itr) {
    // We do not need registered = registered && ... since the loop condition
    // is set to exit early.
    registered = (*itr)->ExistsInHKLM();
  }
  return registered;
}

// This method checks if Chrome is already registered on the local machine
// for the requested protocol. It just checks the one value required for this.
bool IsChromeRegisteredForProtocol(BrowserDistribution* dist,
                                   const std::wstring& suffix,
                                   const std::wstring& protocol) {
  bool registered = true;
  std::list<RegistryEntry*> entries;
  STLElementDeleter<std::list<RegistryEntry*>> entries_deleter(&entries);
  RegistryEntry::GetProtocolCapabilityEntries(dist, suffix, protocol, &entries);
  for (std::list<RegistryEntry*>::const_iterator itr = entries.begin();
       itr != entries.end() && registered; ++itr) {
    // We do not need registered = registered && ... since the loop condition
    // is set to exit early.
    registered = (*itr)->ExistsInHKLM();
  }
  return registered;
}

// This method registers Chrome on Vista by launching an elevated setup.exe.
// That will show the user the standard Vista elevation prompt. If the user
// accepts it the new process will make the necessary changes and return SUCCESS
// that we capture and return.
// If protocol is non-empty we will also register Chrome as being capable of
// handling the protocol.
bool ElevateAndRegisterChrome(BrowserDistribution* dist,
                              const std::wstring& chrome_exe,
                              const std::wstring& suffix,
                              const std::wstring& protocol) {
  FilePath exe_path =
      FilePath::FromWStringHack(chrome_exe).DirName()
          .Append(installer::kSetupExe);
  if (!file_util::PathExists(exe_path)) {
    HKEY reg_root = InstallUtil::IsPerUserInstall(chrome_exe.c_str()) ?
        HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
    RegKey key(reg_root, dist->GetUninstallRegPath().c_str(), KEY_READ);
    std::wstring uninstall_string;
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

// This method tries to figure out if another user has already registered her
// own copy of Chrome so that we can avoid overwriting it and append current
// user's login name to default browser registry entries. This function is
// not meant to detect all cases. It just tries to handle the most common case.
// All the conditions below have to be true for it to return true:
// - Software\Clients\StartMenuInternet\Chromium\"" key should have a valid
//   value.
// - The value should not be same as given value in |chrome_exe|
// - Finally to handle the default install path (C:\Document and Settings\
//   <user>\Local Settings\Application Data\Chromium\Application) the value
//   of the above key should differ from |chrome_exe| only in user name.
bool AnotherUserHasDefaultBrowser(BrowserDistribution* dist,
                                  const std::wstring& chrome_exe) {
  const std::wstring reg_key(
      RegistryEntry::GetBrowserClientKey(dist, std::wstring())
      .append(ShellUtil::kRegShellOpen));
  RegKey key(HKEY_LOCAL_MACHINE, reg_key.c_str(), KEY_READ);
  std::wstring registry_chrome_exe;
  if ((key.ReadValue(L"", &registry_chrome_exe) != ERROR_SUCCESS) ||
      registry_chrome_exe.length() < 2)
    return false;

  registry_chrome_exe = registry_chrome_exe.substr(1,
      registry_chrome_exe.length() - 2);
  if ((registry_chrome_exe.size() == chrome_exe.size()) &&
      (std::equal(chrome_exe.begin(), chrome_exe.end(),
                  registry_chrome_exe.begin(),
                  base::CaseInsensitiveCompare<wchar_t>()))) {
    return false;
  }

  std::vector<std::wstring> v1, v2;
  base::SplitString(registry_chrome_exe, L'\\', &v1);
  base::SplitString(chrome_exe, L'\\', &v2);
  if (v1.empty() || v2.empty() || v1.size() != v2.size())
    return false;

  // Now check that only one of the values within two '\' chars differ.
  std::vector<std::wstring>::iterator itr1 = v1.begin();
  std::vector<std::wstring>::iterator itr2 = v2.begin();
  bool one_mismatch = false;
  for ( ; itr1 < v1.end() && itr2 < v2.end(); ++itr1, ++itr2) {
    std::wstring s1 = *itr1;
    std::wstring s2 = *itr2;
    if ((s1.size() != s2.size()) ||
        (!std::equal(s1.begin(), s1.end(),
                     s2.begin(), base::CaseInsensitiveCompare<wchar_t>()))) {
      if (one_mismatch)
        return false;
      else
        one_mismatch = true;
    }
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

#if defined(GOOGLE_CHROME_BUILD)
const wchar_t* ShellUtil::kChromeHTMLProgId = L"ChromeHTML";
const wchar_t* ShellUtil::kChromeHTMLProgIdDesc = L"Chrome HTML Document";
#else
const wchar_t* ShellUtil::kChromeHTMLProgId = L"ChromiumHTML";
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

bool ShellUtil::AdminNeededForRegistryCleanup(BrowserDistribution* dist,
                                              const std::wstring& suffix) {
  bool cleanup_needed = false;
  std::list<RegistryEntry*> entries;
  STLElementDeleter<std::list<RegistryEntry*>> entries_deleter(&entries);
  RegistryEntry::GetProgIdEntries(dist, L"chrome.exe", suffix, &entries);
  RegistryEntry::GetSystemEntries(dist, L"chrome.exe", suffix, &entries);
  for (std::list<RegistryEntry*>::const_iterator itr = entries.begin();
       itr != entries.end() && !cleanup_needed; ++itr) {
    cleanup_needed = (*itr)->NameExistsInHKLM();
  }
  return cleanup_needed;
}

bool ShellUtil::CreateChromeDesktopShortcut(BrowserDistribution* dist,
                                            const std::wstring& chrome_exe,
                                            const std::wstring& description,
                                            const std::wstring& appended_name,
                                            const std::wstring& arguments,
                                            const std::wstring& icon_path,
                                            int icon_index,
                                            ShellChange shell_change,
                                            bool alternate,
                                            bool create_new) {
  std::wstring shortcut_name;
  if (!ShellUtil::GetChromeShortcutName(dist, alternate, appended_name,
                                        &shortcut_name))
    return false;

  bool ret = false;
  if (shell_change == ShellUtil::CURRENT_USER) {
    FilePath shortcut_path;
    // We do not want to create a desktop shortcut to Chrome in the current
    // user's desktop folder if there is already one in the "All Users"
    // desktop folder.
    bool got_system_desktop = ShellUtil::GetDesktopPath(true, &shortcut_path);
    FilePath shortcut = shortcut_path.Append(shortcut_name);
    if (!got_system_desktop || !file_util::PathExists(shortcut)) {
      // Either we couldn't query the "All Users" Desktop folder or there's
      // nothing in it, so let's continue.
      if (ShellUtil::GetDesktopPath(false, &shortcut_path)) {
        shortcut = shortcut_path.Append(shortcut_name);
        ret = ShellUtil::UpdateChromeShortcut(dist,
                                              chrome_exe,
                                              shortcut.value(),
                                              arguments,
                                              description,
                                              icon_path,
                                              icon_index,
                                              create_new);
      }
    }
  } else if (shell_change == ShellUtil::SYSTEM_LEVEL) {
    FilePath shortcut_path;
    if (ShellUtil::GetDesktopPath(true, &shortcut_path)) {
      FilePath shortcut = shortcut_path.Append(shortcut_name);
      ret = ShellUtil::UpdateChromeShortcut(dist,
                                            chrome_exe,
                                            shortcut.value(),
                                            arguments,
                                            description,
                                            icon_path,
                                            icon_index,
                                            create_new);
    }
  } else {
    NOTREACHED();
  }
  return ret;
}

bool ShellUtil::CreateChromeQuickLaunchShortcut(BrowserDistribution* dist,
                                                const std::wstring& chrome_exe,
                                                int shell_change,
                                                bool create_new) {
  std::wstring shortcut_name;
  if (!ShellUtil::GetChromeShortcutName(dist, false, L"", &shortcut_name))
    return false;

  bool ret = true;
  // First create shortcut for the current user.
  if (shell_change & ShellUtil::CURRENT_USER) {
    std::wstring user_ql_path;
    if (ShellUtil::GetQuickLaunchPath(false, &user_ql_path)) {
      file_util::AppendToPath(&user_ql_path, shortcut_name);
      ret = ShellUtil::UpdateChromeShortcut(dist, chrome_exe, user_ql_path,
                                            L"", L"", chrome_exe,
                                            dist->GetIconIndex(),
                                            create_new);
    } else {
      ret = false;
    }
  }

  // Add a shortcut to Default User's profile so that all new user profiles
  // get it.
  if (shell_change & ShellUtil::SYSTEM_LEVEL) {
    std::wstring default_ql_path;
    if (ShellUtil::GetQuickLaunchPath(true, &default_ql_path)) {
      file_util::AppendToPath(&default_ql_path, shortcut_name);
      ret = ShellUtil::UpdateChromeShortcut(dist, chrome_exe, default_ql_path,
                                            L"", L"", chrome_exe,
                                            dist->GetIconIndex(),
                                            create_new) && ret;
    } else {
      ret = false;
    }
  }

  return ret;
}

std::wstring ShellUtil::GetChromeIcon(BrowserDistribution* dist,
                                      const std::wstring& chrome_exe) {
  std::wstring chrome_icon(chrome_exe);
  chrome_icon.append(L",");
  chrome_icon.append(base::IntToString16(dist->GetIconIndex()));
  return chrome_icon;
}

std::wstring ShellUtil::GetChromeShellOpenCmd(const std::wstring& chrome_exe) {
  return L"\"" + chrome_exe + L"\" -- \"%1\"";
}

bool ShellUtil::GetChromeShortcutName(BrowserDistribution* dist,
                                      bool alternate,
                                      const std::wstring& appended_name,
                                      std::wstring* shortcut) {
  shortcut->assign(alternate ? dist->GetAlternateApplicationName() :
                               dist->GetAppShortCutName());
  if (!appended_name.empty()) {
    shortcut->append(L" (");
    shortcut->append(appended_name);
    shortcut->append(L")");
  }
  shortcut->append(L".lnk");
  return true;
}

bool ShellUtil::GetDesktopPath(bool system_level, FilePath* path) {
  wchar_t desktop[MAX_PATH];
  int dir = system_level ? CSIDL_COMMON_DESKTOPDIRECTORY :
                           CSIDL_DESKTOPDIRECTORY;
  if (FAILED(SHGetFolderPath(NULL, dir, NULL, SHGFP_TYPE_CURRENT, desktop)))
    return false;
  *path = FilePath(desktop);
  return true;
}

bool ShellUtil::GetQuickLaunchPath(bool system_level, std::wstring* path) {
  static const wchar_t* kQuickLaunchPath =
      L"Microsoft\\Internet Explorer\\Quick Launch";
  wchar_t qlaunch[MAX_PATH];
  if (system_level) {
    // We are accessing GetDefaultUserProfileDirectory this way so that we do
    // not have to declare dependency to Userenv.lib for chrome.exe
    typedef BOOL (WINAPI *PROFILE_FUNC)(LPWSTR, LPDWORD);
    HMODULE module = LoadLibrary(L"Userenv.dll");
    PROFILE_FUNC p = reinterpret_cast<PROFILE_FUNC>(GetProcAddress(module,
        "GetDefaultUserProfileDirectoryW"));
    DWORD size = _countof(qlaunch);
    if ((p == NULL) || ((p)(qlaunch, &size) != TRUE))
      return false;
    *path = qlaunch;
    if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
      file_util::AppendToPath(path, L"AppData\\Roaming");
    } else {
      file_util::AppendToPath(path, L"Application Data");
    }
  } else {
    if (FAILED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL,
                               SHGFP_TYPE_CURRENT, qlaunch)))
      return false;
    *path = qlaunch;
  }
  file_util::AppendToPath(path, kQuickLaunchPath);
  return true;
}

void ShellUtil::GetRegisteredBrowsers(
    BrowserDistribution* dist,
    std::map<std::wstring, std::wstring>* browsers) {
  const HKEY root = HKEY_LOCAL_MACHINE;
  const std::wstring base_key(ShellUtil::kRegStartMenuInternet);
  std::wstring client_path;
  RegKey key;
  std::wstring name;
  std::wstring command;
  for (base::win::RegistryKeyIterator iter(root, base_key.c_str());
       iter.Valid(); ++iter) {
    client_path.assign(base_key).append(1, L'\\').append(iter.Name());
    // Read the browser's name (localized according to install language).
    if (key.Open(root, client_path.c_str(), KEY_QUERY_VALUE) != ERROR_SUCCESS ||
        key.ReadValue(NULL, &name) != ERROR_SUCCESS) {
      continue;
    }
    // Read the browser's reinstall command.
    if (key.Open(root, (client_path + L"\\InstallInfo").c_str(),
                 KEY_QUERY_VALUE) != ERROR_SUCCESS ||
        key.ReadValue(kReinstallCommand, &command) != ERROR_SUCCESS) {
      continue;
    }
    if (!name.empty() && !command.empty() &&
        name.find(dist->GetApplicationName()) == std::wstring::npos)
      (*browsers)[name] = command;
  }
}

bool ShellUtil::GetUserSpecificDefaultBrowserSuffix(BrowserDistribution* dist,
                                                    std::wstring* entry) {
  wchar_t user_name[256];
  DWORD size = arraysize(user_name);
  if (::GetUserName(user_name, &size) == 0 || size < 1)
    return false;
  entry->reserve(size);
  entry->assign(1, L'.');
  entry->append(user_name, size - 1);

  return RegKey(HKEY_LOCAL_MACHINE,
                RegistryEntry::GetBrowserClientKey(dist, *entry).c_str(),
                KEY_READ).Valid();
}

bool ShellUtil::MakeChromeDefault(BrowserDistribution* dist,
                                  int shell_change,
                                  const std::wstring& chrome_exe,
                                  bool elevate_if_not_admin) {
  if (!dist->CanSetAsDefault())
    return false;

  ShellUtil::RegisterChromeBrowser(dist, chrome_exe, L"", elevate_if_not_admin);

  bool ret = true;
  // First use the new "recommended" way on Vista to make Chrome default
  // browser.
  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    VLOG(1) << "Registering Chrome as default browser on Vista.";
    base::win::ScopedComPtr<IApplicationAssociationRegistration> pAAR;
    HRESULT hr = pAAR.CreateInstance(CLSID_ApplicationAssociationRegistration,
        NULL, CLSCTX_INPROC);
    if (SUCCEEDED(hr)) {
      std::wstring app_name = dist->GetApplicationName();
      std::wstring suffix;
      if (ShellUtil::GetUserSpecificDefaultBrowserSuffix(dist, &suffix))
        app_name += suffix;

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

  // Now use the old way to associate Chrome with supported protocols and file
  // associations. This should not be required on Vista but since some
  // applications still read Software\Classes\http key directly, we have to do
  // this on Vista also.

  std::list<RegistryEntry*> entries;
  STLElementDeleter<std::list<RegistryEntry*>> entries_deleter(&entries);
  std::wstring suffix;
  if (!GetUserSpecificDefaultBrowserSuffix(dist, &suffix))
    suffix = L"";
  RegistryEntry::GetUserEntries(dist, chrome_exe, suffix, &entries);
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

  // Send Windows notification event so that it can update icons for
  // file associations.
  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
  return ret;
}

bool ShellUtil::MakeChromeDefaultProtocolClient(BrowserDistribution* dist,
                                                const std::wstring& chrome_exe,
                                                const std::wstring& protocol) {
  if (!dist->CanSetAsDefault())
    return false;

  ShellUtil::RegisterChromeForProtocol(dist, chrome_exe, L"", protocol, true);

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
      std::wstring app_name = dist->GetApplicationName();
      std::wstring suffix;
      if (ShellUtil::GetUserSpecificDefaultBrowserSuffix(dist, &suffix))
        app_name += suffix;

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
  // should not be required on Vista but since some applications still read
  // Software\Classes\http key directly, we have to do this on Vista also.

  std::list<RegistryEntry*> entries;
  STLElementDeleter<std::list<RegistryEntry*>> entries_deleter(&entries);
  std::wstring suffix;
  if (!GetUserSpecificDefaultBrowserSuffix(dist, &suffix))
    suffix = L"";
  std::wstring chrome_open = ShellUtil::GetChromeShellOpenCmd(chrome_exe);
  std::wstring chrome_icon = ShellUtil::GetChromeIcon(dist, chrome_exe);
  RegistryEntry::GetUserProtocolEntries(protocol, chrome_icon, chrome_open,
                                        &entries);
  // Change the default protocol handler for current user.
  if (!AddRegistryEntries(HKEY_CURRENT_USER, entries)) {
      ret = false;
      LOG(ERROR) << "Could not make Chrome default protocol client (XP).";
  }

  return ret;
}

bool ShellUtil::RegisterChromeBrowser(BrowserDistribution* dist,
                                      const std::wstring& chrome_exe,
                                      const std::wstring& unique_suffix,
                                      bool elevate_if_not_admin) {
  if (!dist->CanSetAsDefault())
    return false;

  // First figure out we need to append a suffix to the registry entries to
  // make them unique.
  std::wstring suffix;
  if (!unique_suffix.empty()) {
    suffix = unique_suffix;
  } else if (InstallUtil::IsPerUserInstall(chrome_exe.c_str()) &&
             !GetUserSpecificDefaultBrowserSuffix(dist, &suffix) &&
             !AnotherUserHasDefaultBrowser(dist, chrome_exe)) {
    suffix = L"";
  }

  // Check if Chromium is already registered with this suffix.
  if (IsChromeRegistered(dist, chrome_exe, suffix))
    return true;

  // If user is an admin try to register and return the status.
  if (IsUserAnAdmin()) {
    std::list<RegistryEntry*> entries;
    STLElementDeleter<std::list<RegistryEntry*>> entries_deleter(&entries);
    RegistryEntry::GetProgIdEntries(dist, chrome_exe, suffix, &entries);
    RegistryEntry::GetSystemEntries(dist, chrome_exe, suffix, &entries);
    return AddRegistryEntries(HKEY_LOCAL_MACHINE, entries);
  }

  // If user is not an admin and OS is Vista, try to elevate and register.
  if (elevate_if_not_admin &&
      base::win::GetVersion() >= base::win::VERSION_VISTA &&
      ElevateAndRegisterChrome(dist, chrome_exe, suffix, L""))
    return true;

  // If we got to this point then all we can do is create ProgIds under HKCU
  // on XP as well as Vista.
  std::list<RegistryEntry*> entries;
  STLElementDeleter<std::list<RegistryEntry*>> entries_deleter(&entries);
  RegistryEntry::GetProgIdEntries(dist, chrome_exe, L"", &entries);
  return AddRegistryEntries(HKEY_CURRENT_USER, entries);
}

bool ShellUtil::RegisterChromeForProtocol(BrowserDistribution* dist,
                                          const std::wstring& chrome_exe,
                                          const std::wstring& unique_suffix,
                                          const std::wstring& protocol,
                                          bool elevate_if_not_admin) {
  if (!dist->CanSetAsDefault())
    return false;

  // Figure out we need to append a suffix to the registry entries to
  // make them unique.
  std::wstring suffix;
  if (!unique_suffix.empty()) {
    suffix = unique_suffix;
  } else if (InstallUtil::IsPerUserInstall(chrome_exe.c_str()) &&
             !GetUserSpecificDefaultBrowserSuffix(dist, &suffix) &&
             !AnotherUserHasDefaultBrowser(dist, chrome_exe)) {
    suffix = L"";
  }

  // Check if Chromium is already registered with this suffix.
  if (IsChromeRegisteredForProtocol(dist, suffix, protocol))
    return true;

  if (IsUserAnAdmin()) {
    // We can do this operation directly.
    // If we're not registered at all, try to register. If that fails
    // we should give up.
    if (!IsChromeRegistered(dist, chrome_exe, suffix) &&
        !RegisterChromeBrowser(dist, chrome_exe, suffix, false)) {
      return false;
    }

    // Write in the capabillity for the protocol.
    std::list<RegistryEntry*> entries;
    STLElementDeleter<std::list<RegistryEntry*>> entries_deleter(&entries);
    RegistryEntry::GetProtocolCapabilityEntries(dist, suffix, protocol,
        &entries);
    return AddRegistryEntries(HKEY_LOCAL_MACHINE, entries);
  } else if (elevate_if_not_admin &&
             base::win::GetVersion() >= base::win::VERSION_VISTA) {
    // Elevate to do the whole job
    return ElevateAndRegisterChrome(dist, chrome_exe, suffix, protocol);
  } else {
    // we need admin rights to register our capability. If we don't
    // have them and can't elevate, give up.
    return false;
  }
}

bool ShellUtil::RemoveChromeDesktopShortcut(BrowserDistribution* dist,
                                            int shell_change, bool alternate) {
  std::wstring shortcut_name;
  if (!ShellUtil::GetChromeShortcutName(dist, alternate, L"",
                                        &shortcut_name))
    return false;

  bool ret = true;
  if (shell_change & ShellUtil::CURRENT_USER) {
    FilePath shortcut_path;
    if (ShellUtil::GetDesktopPath(false, &shortcut_path)) {
      FilePath shortcut = shortcut_path.Append(shortcut_name);
      ret = file_util::Delete(shortcut, false);
    } else {
      ret = false;
    }
  }

  if (shell_change & ShellUtil::SYSTEM_LEVEL) {
    FilePath shortcut_path;
    if (ShellUtil::GetDesktopPath(true, &shortcut_path)) {
      FilePath shortcut = shortcut_path.Append(shortcut_name);
      ret = file_util::Delete(shortcut, false) && ret;
    } else {
      ret = false;
    }
  }
  return ret;
}

bool ShellUtil::RemoveChromeDesktopShortcutsWithAppendedNames(
    const std::vector<std::wstring>& appended_names) {
  FilePath shortcut_path;
  bool ret = true;
  if (ShellUtil::GetDesktopPath(false, &shortcut_path)) {
    for (std::vector<std::wstring>::const_iterator it =
             appended_names.begin();
         it != appended_names.end();
         ++it) {
      FilePath delete_shortcut = shortcut_path.Append(*it);
      ret = ret && file_util::Delete(delete_shortcut, false);
    }
  } else {
    ret = false;
  }
  return ret;
}

bool ShellUtil::RemoveChromeQuickLaunchShortcut(BrowserDistribution* dist,
                                                int shell_change) {
  std::wstring shortcut_name;
  if (!ShellUtil::GetChromeShortcutName(dist, false, L"", &shortcut_name))
    return false;

  bool ret = true;
  // First remove shortcut for the current user.
  if (shell_change & ShellUtil::CURRENT_USER) {
    std::wstring user_ql_path;
    if (ShellUtil::GetQuickLaunchPath(false, &user_ql_path)) {
      file_util::AppendToPath(&user_ql_path, shortcut_name);
      ret = file_util::Delete(user_ql_path, false);
    } else {
      ret = false;
    }
  }

  // Delete shortcut in Default User's profile
  if (shell_change & ShellUtil::SYSTEM_LEVEL) {
    std::wstring default_ql_path;
    if (ShellUtil::GetQuickLaunchPath(true, &default_ql_path)) {
      file_util::AppendToPath(&default_ql_path, shortcut_name);
      ret = file_util::Delete(default_ql_path, false) && ret;
    } else {
      ret = false;
    }
  }

  return ret;
}

bool ShellUtil::UpdateChromeShortcut(BrowserDistribution* dist,
                                     const std::wstring& chrome_exe,
                                     const std::wstring& shortcut,
                                     const std::wstring& arguments,
                                     const std::wstring& description,
                                     const std::wstring& icon_path,
                                     int icon_index,
                                     bool create_new) {
  std::wstring chrome_path = FilePath(chrome_exe).DirName().value();

  FilePath prefs_path(chrome_path);
  prefs_path = prefs_path.AppendASCII(installer::kDefaultMasterPrefs);
  installer::MasterPreferences prefs(prefs_path);
  if (FilePath::CompareEqualIgnoreCase(icon_path, chrome_exe)) {
    prefs.GetInt(installer::master_preferences::kChromeShortcutIconIndex,
                 &icon_index);
  }
  if (create_new) {
    return file_util::CreateShortcutLink(
        chrome_exe.c_str(),                // target
        shortcut.c_str(),                  // shortcut
        chrome_path.c_str(),               // working dir
        arguments.c_str(),                 // arguments
        description.c_str(),               // description
        icon_path.c_str(),                 // icon file
        icon_index,                        // icon index
        dist->GetBrowserAppId().c_str());  // app id
  } else {
    return file_util::UpdateShortcutLink(
        chrome_exe.c_str(),                // target
        shortcut.c_str(),                  // shortcut
        chrome_path.c_str(),               // working dir
        arguments.c_str(),                 // arguments
        description.c_str(),               // description
        icon_path.c_str(),                 // icon file
        icon_index,                        // icon index
        dist->GetBrowserAppId().c_str());  // app id
  }
}
