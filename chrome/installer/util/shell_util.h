// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares methods that are useful for integrating Chrome in
// Windows shell. These methods are all static and currently part of
// ShellUtil class.

#ifndef CHROME_INSTALLER_UTIL_SHELL_UTIL_H_
#define CHROME_INSTALLER_UTIL_SHELL_UTIL_H_
#pragma once

#include <windows.h>
#include <map>
#include <string>

#include "base/basictypes.h"
#include "chrome/installer/util/work_item_list.h"

class BrowserDistribution;
class FilePath;

namespace base {
class DictionaryValue;
}

// This is a utility class that provides common shell integration methods
// that can be used by installer as well as Chrome.
class ShellUtil {
 public:
  // Input to any methods that make changes to OS shell.
  enum ShellChange {
    CURRENT_USER = 0x1,  // Make any shell changes only at the user level
    SYSTEM_LEVEL = 0x2   // Make any shell changes only at the system level
  };

  // Relative path of the URL Protocol registry entry (prefixed with '\').
  static const wchar_t* kRegURLProtocol;

  // Relative path of DefaultIcon registry entry (prefixed with '\').
  static const wchar_t* kRegDefaultIcon;

  // Relative path of "shell" registry key.
  static const wchar_t* kRegShellPath;

  // Relative path of shell open command in Windows registry
  // (i.e. \\shell\\open\\command).
  static const wchar_t* kRegShellOpen;

  // Relative path of registry key under which applications need to register
  // to control Windows Start menu links.
  static const wchar_t* kRegStartMenuInternet;

  // Relative path of Classes registry entry under which file associations
  // are added on Windows.
  static const wchar_t* kRegClasses;

  // Relative path of RegisteredApplications registry entry under which
  // we add Chrome as a Windows application
  static const wchar_t* kRegRegisteredApplications;

  // The key path and key name required to register Chrome on Windows such
  // that it can be launched from Start->Run just by name (chrome.exe).
  static const wchar_t* kAppPathsRegistryKey;
  static const wchar_t* kAppPathsRegistryPathName;

  // Name that we give to Chrome file association handler ProgId.
  static const wchar_t* kChromeHTMLProgId;

  // Description of Chrome file association handler ProgId.
  static const wchar_t* kChromeHTMLProgIdDesc;

  // Registry path that stores url associations on Vista.
  static const wchar_t* kRegVistaUrlPrefs;

  // File extensions that Chrome registers itself for.
  static const wchar_t* kFileAssociations[];

  // Protocols that Chrome registers itself as the default handler for
  // when the user makes Chrome the default browser.
  static const wchar_t* kBrowserProtocolAssociations[];

  // Protocols that Chrome registers itself as being capable of handling.
  static const wchar_t* kPotentialProtocolAssociations[];

  // Registry value name that is needed for ChromeHTML ProgId
  static const wchar_t* kRegUrlProtocol;

  // Checks if we need Admin rights for registry cleanup by checking if any
  // entry exists in HKLM.
  static bool AdminNeededForRegistryCleanup(BrowserDistribution* dist,
                                            const std::wstring& suffix);

  // Creates Chrome shortcut on the Desktop.
  // |dist| gives the type of browser distribution currently in use.
  // |chrome_exe| provides the target path information.
  // |description| provides the shortcut's "comment" property.
  // |appended_name| provides a string to be appended to the distribution name,
  //     and can be the empty string.
  // |arguments| gives a set of arguments to be passed to the executable.
  // |icon_path| provides the path to the icon file to use.
  // |icon_index| provides the index of the icon within the provided icon file.
  // If |shell_change| is CURRENT_USER, the shortcut is created in the
  //     Desktop folder of current user's profile.
  // If |shell_change| is SYSTEM_LEVEL, the shortcut is created in the
  //     Desktop folder of the "All Users" profile.
  // If |alternate| is true, an alternate text for the shortcut is used.
  // If |create_new| is false, an existing shortcut will be updated, but if
  //     no shortcut exists, it will not be created.
  // Returns true iff the method causes a shortcut to be created / updated.
  static bool CreateChromeDesktopShortcut(BrowserDistribution* dist,
                                          const std::wstring& chrome_exe,
                                          const std::wstring& description,
                                          const std::wstring& appended_name,
                                          const std::wstring& arguments,
                                          const std::wstring& icon_path,
                                          int icon_index,
                                          ShellChange shell_change,
                                          bool alternate,
                                          bool create_new);

  // Create Chrome shortcut on Quick Launch Bar.
  // If shell_change is CURRENT_USER, the shortcut is created in the
  // Quick Launch folder of current user's profile.
  // If shell_change is SYSTEM_LEVEL, the shortcut is created in the
  // Quick Launch folder of "Default User" profile. This will make sure
  // that this shortcut will be seen by all the new users logging into the
  // system.
  // create_new: If false, will only update the shortcut. If true, the function
  //             will create a new shortcut if it doesn't exist already.
  static bool CreateChromeQuickLaunchShortcut(BrowserDistribution* dist,
                                              const std::wstring& chrome_exe,
                                              int shell_change,
                                              bool create_new);

  // This method appends the Chrome icon index inside chrome.exe to the
  // chrome.exe path passed in as input, to generate the full path for
  // Chrome icon that can be used as value for Windows registry keys.
  // |chrome_exe| full path to chrome.exe.
  static std::wstring GetChromeIcon(BrowserDistribution* dist,
                                    const std::wstring& chrome_exe);

  // This method returns the command to open URLs/files using chrome. Typically
  // this command is written to the registry under shell\open\command key.
  // chrome_exe: the full path to chrome.exe
  static std::wstring GetChromeShellOpenCmd(const std::wstring& chrome_exe);

  // Returns the localized name of Chrome shortcut in |shortcut|. If
  // |appended_name| is not empty, it is included in the shortcut name. If
  // |alternate| is true, a second localized text that is better suited for
  // certain scenarios is used.
  static bool GetChromeShortcutName(BrowserDistribution* dist,
                                    bool alternate,
                                    const std::wstring& appended_name,
                                    std::wstring* shortcut);

  // Gets the desktop path for the current user or all users (if system_level
  // is true) and returns it in 'path' argument. Return true if successful,
  // otherwise returns false.
  static bool GetDesktopPath(bool system_level, FilePath* path);

  // Gets the Quick Launch shortcuts path for the current user and
  // returns it in 'path' argument. Return true if successful, otherwise
  // returns false. If system_level is true this function returns the path
  // to Default Users Quick Launch shortcuts path. Adding a shortcut to Default
  // User's profile only affects any new user profiles (not existing ones).
  static bool GetQuickLaunchPath(bool system_level, std::wstring* path);

  // Gets a mapping of all registered browser (on local machine) names and
  // their reinstall command (which usually sets browser as default).
  static void GetRegisteredBrowsers(BrowserDistribution* dist,
                                    std::map<std::wstring,
                                    std::wstring>* browsers);

  // This function gets a suffix (user's login name) that can be added
  // to Chromium default browser entry in the registry to create a unique name
  // if there are multiple users on the machine, each with their own copy of
  // Chromium that they want to set as default browser.
  // This suffix value is assigned to |entry|. The function also checks for
  // existence of Default Browser registry key with this suffix and
  // returns true if it exists. In all other cases it returns false.
  static bool GetUserSpecificDefaultBrowserSuffix(BrowserDistribution* dist,
                                                  std::wstring* entry);

  // Make Chrome the default browser. This function works by going through
  // the url protocols and file associations that are related to general
  // browsing, e.g. http, https, .html etc., and requesting to become the
  // default handler for each. If any of these fails the operation will return
  // false to indicate failure, which is consistent with the return value of
  // ShellIntegration::IsDefaultBrowser.
  //
  // In the case of failure any successful changes will be left, however no
  // more changes will be attempted.
  // TODO(benwells): Attempt to undo any changes that were successfully made.
  // http://crbug.com/83970
  //
  // shell_change: Defined whether to register as default browser at system
  //               level or user level. If value has ShellChange::SYSTEM_LEVEL
  //               we should be running as admin user.
  // chrome_exe: The chrome.exe path to register as default browser.
  // elevate_if_not_admin: On Vista if user is not admin, try to elevate for
  //                       Chrome registration.
  static bool MakeChromeDefault(BrowserDistribution* dist,
                                int shell_change,
                                const std::wstring& chrome_exe,
                                bool elevate_if_not_admin);

  // Make Chrome the default application for a protocol.
  // chrome_exe: The chrome.exe path to register as default browser.
  // protocol: The protocol to register as the default handler for.
  static bool MakeChromeDefaultProtocolClient(BrowserDistribution* dist,
                                              const std::wstring& chrome_exe,
                                              const std::wstring& protocol);

  // This method adds Chrome to the list that shows up in Add/Remove Programs->
  // Set Program Access and Defaults and also creates Chrome ProgIds under
  // Software\Classes. This method requires write access to HKLM so is just
  // best effort deal. If write to HKLM fails and elevate_if_not_admin is true,
  // this method will:
  // - add the ProgId entries to HKCU on XP. HKCU entries will not make
  //   Chrome show in Set Program Access and Defaults but they are still useful
  //   because we can make Chrome run when user clicks on http link or html
  //   file.
  // - will try to launch setup.exe with admin priviledges on Vista to do
  //   these tasks. Users will see standard Vista elevation prompt and if they
  //   enter the right credentials, the write operation will work.
  // Currently elevate_if_not_admin is true only when user tries to make Chrome
  // default browser (through the UI or through installer options) and Chrome
  // is not registered on the machine.
  //
  // |chrome_exe| full path to chrome.exe.
  // |unique_suffix| Optional input. If given, this function appends the value
  // to default browser entries names that it creates in the registry.
  // |elevate_if_not_admin| if true will make this method try alternate methods
  // as described above.
  static bool RegisterChromeBrowser(BrowserDistribution* dist,
                                    const std::wstring& chrome_exe,
                                    const std::wstring& unique_suffix,
                                    bool elevate_if_not_admin);

  // This method declares to Windows that Chrome is capable of handling the
  // given protocol. This function will call the RegisterChromeBrowser function
  // to register with Windows as capable of handling the protocol, if it isn't
  // currently registered as capable.
  // Declaring the capability of handling a protocol is necessary to register
  // as the default handler for the protocol in Vista and later versions of
  // Windows.
  //
  // If called by the browser and elevation is required, it will elevate by
  // calling setup.exe which will again call this function with elevate false.
  //
  // |chrome_exe| full path to chrome.exe.
  // |unique_suffix| Optional input. If given, this function appends the value
  // to default browser entries names that it creates in the registry.
  // |protocol| The protocol to register as being capable of handling.s
  // |elevate_if_not_admin| if true will make this method try alternate methods
  // as described above.
  static bool RegisterChromeForProtocol(BrowserDistribution* dist,
                                        const std::wstring& chrome_exe,
                                        const std::wstring& unique_suffix,
                                        const std::wstring& protocol,
                                        bool elevate_if_not_admin);

  // Remove Chrome shortcut from Desktop.
  // If shell_change is CURRENT_USER, the shortcut is removed from the
  // Desktop folder of current user's profile.
  // If shell_change is SYSTEM_LEVEL, the shortcut is removed from the
  // Desktop folder of "All Users" profile.
  // If alternate is true, the shortcut with the alternate name is removed. See
  // CreateChromeDesktopShortcut() for more information.
  static bool RemoveChromeDesktopShortcut(BrowserDistribution* dist,
                                          int shell_change,
                                          bool alternate);

  // Removes a set of existing Chrome desktop shortcuts. |appended_names| is a
  // list of shortcut file names as obtained from
  // ShellUtil::GetChromeShortcutName.
  static bool RemoveChromeDesktopShortcutsWithAppendedNames(
      const std::vector<std::wstring>& appended_names);

  // Remove Chrome shortcut from Quick Launch Bar.
  // If shell_change is CURRENT_USER, the shortcut is removed from
  // the Quick Launch folder of current user's profile.
  // If shell_change is SYSTEM_LEVEL, the shortcut is removed from
  // the Quick Launch folder of "Default User" profile.
  static bool RemoveChromeQuickLaunchShortcut(BrowserDistribution* dist,
                                              int shell_change);

  // Updates shortcut (or creates a new shortcut) at destination given by
  // shortcut to a target given by chrome_exe. The arguments are given by
  // |arguments| for the target and icon is set based on |icon_path| and
  // |icon_index|. If create_new is set to true, the function will create a new
  // shortcut if it doesn't exist.
  static bool UpdateChromeShortcut(BrowserDistribution* dist,
                                   const std::wstring& chrome_exe,
                                   const std::wstring& shortcut,
                                   const std::wstring& arguments,
                                   const std::wstring& description,
                                   const std::wstring& icon_path,
                                   int icon_index,
                                   bool create_new);

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellUtil);
};


#endif  // CHROME_INSTALLER_UTIL_SHELL_UTIL_H_
