// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares methods that are useful for integrating Chrome in
// Windows shell. These methods are all static and currently part of
// ShellUtil class.

#ifndef CHROME_INSTALLER_UTIL_SHELL_UTIL_H_
#define CHROME_INSTALLER_UTIL_SHELL_UTIL_H_

#include <windows.h>

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
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

  enum VerifyShortcutStatus {
    VERIFY_SHORTCUT_SUCCESS = 0,
    VERIFY_SHORTCUT_FAILURE_UNEXPECTED,
    VERIFY_SHORTCUT_FAILURE_PATH,
    VERIFY_SHORTCUT_FAILURE_DESCRIPTION,
    VERIFY_SHORTCUT_FAILURE_ICON_INDEX,
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

  // Relative registry path from \Software\Classes\ChromeHTML to the ProgId
  // Application definitions.
  static const wchar_t* kRegApplication;

  // Registry value name for the AppUserModelId of an application.
  static const wchar_t* kRegAppUserModelId;

  // Registry value name for the description of an application.
  static const wchar_t* kRegApplicationDescription;

  // Registry value name for an application's name.
  static const wchar_t* kRegApplicationName;

  // Registry value name for the path to an application's icon.
  static const wchar_t* kRegApplicationIcon;

  // Registry value name for an application's company.
  static const wchar_t* kRegApplicationCompany;

  // Relative path of ".exe" registry key.
  static const wchar_t* kRegExePath;

  // Registry value name of the open verb.
  static const wchar_t* kRegVerbOpen;

  // Registry value name of the opennewwindow verb.
  static const wchar_t* kRegVerbOpenNewWindow;

  // Registry value name of the run verb.
  static const wchar_t* kRegVerbRun;

  // Registry value name for command entries.
  static const wchar_t* kRegCommand;

  // Registry value name for the DelegateExecute verb handler.
  static const wchar_t* kRegDelegateExecute;

  // Registry value name for the OpenWithProgids entry for file associations.
  static const wchar_t* kRegOpenWithProgids;

  // Returns true if |chrome_exe| is registered in HKLM with |suffix|.
  // Note: This only checks one deterministic key in HKLM for |chrome_exe| and
  // doesn't otherwise validate a full Chrome install in HKLM.
  static bool QuickIsChromeRegisteredInHKLM(BrowserDistribution* dist,
                                            const string16& chrome_exe,
                                            const string16& suffix);

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
  // |options|: bitfield for which the options come from ChromeShortcutOptions.
  // Returns true iff the method causes a shortcut to be created / updated.
  static bool CreateChromeDesktopShortcut(BrowserDistribution* dist,
                                          const string16& chrome_exe,
                                          const string16& description,
                                          const string16& appended_name,
                                          const string16& arguments,
                                          const string16& icon_path,
                                          int icon_index,
                                          ShellChange shell_change,
                                          uint32 options);

  // Create Chrome shortcut on Quick Launch Bar.
  // If shell_change is CURRENT_USER, the shortcut is created in the
  // Quick Launch folder of current user's profile.
  // If shell_change is SYSTEM_LEVEL, the shortcut is created in the
  // Quick Launch folder of "Default User" profile. This will make sure
  // that this shortcut will be seen by all the new users logging into the
  // system.
  // |options|: bitfield for which the options come from ChromeShortcutOptions.
  static bool CreateChromeQuickLaunchShortcut(BrowserDistribution* dist,
                                              const string16& chrome_exe,
                                              int shell_change,
                                              uint32 options);

  // This method appends the Chrome icon index inside chrome.exe to the
  // chrome.exe path passed in as input, to generate the full path for
  // Chrome icon that can be used as value for Windows registry keys.
  // |chrome_exe| full path to chrome.exe.
  static string16 GetChromeIcon(BrowserDistribution* dist,
                                const string16& chrome_exe);

  // This method returns the command to open URLs/files using chrome. Typically
  // this command is written to the registry under shell\open\command key.
  // |chrome_exe|: the full path to chrome.exe
  static string16 GetChromeShellOpenCmd(const string16& chrome_exe);

  // This method returns the command to be called by the DelegateExecute verb
  // handler to launch chrome on Windows 8. Typically this command is written to
  // the registry under the HKCR\Chrome\.exe\shell\(open|run)\command key.
  // |chrome_exe|: the full path to chrome.exe
  static string16 GetChromeDelegateCommand(const string16& chrome_exe);

  // Returns the localized name of Chrome shortcut in |shortcut|. If
  // |appended_name| is not empty, it is included in the shortcut name. If
  // |alternate| is true, a second localized text that is better suited for
  // certain scenarios is used.
  static bool GetChromeShortcutName(BrowserDistribution* dist,
                                    bool alternate,
                                    const string16& appended_name,
                                    string16* shortcut);

  // Gets the desktop path for the current user or all users (if system_level
  // is true) and returns it in 'path' argument. Return true if successful,
  // otherwise returns false.
  static bool GetDesktopPath(bool system_level, FilePath* path);

  // Gets the Quick Launch shortcuts path for the current user and
  // returns it in 'path' argument. Return true if successful, otherwise
  // returns false. If system_level is true this function returns the path
  // to Default Users Quick Launch shortcuts path. Adding a shortcut to Default
  // User's profile only affects any new user profiles (not existing ones).
  static bool GetQuickLaunchPath(bool system_level, FilePath* path);

  // Gets a mapping of all registered browser names (excluding browsers in the
  // |dist| distribution) and their reinstall command (which usually sets
  // browser as default).
  // Given browsers can be registered in HKCU (as of Win7) and/or in HKLM, this
  // method looks in both and gives precedence to values in HKCU as per the msdn
  // standard: http://goo.gl/xjczJ.
  static void GetRegisteredBrowsers(BrowserDistribution* dist,
                                    std::map<string16, string16>* browsers);

  // Returns the suffix this user's Chrome install is registered with.
  // Always returns the empty string on system-level installs.
  //
  // This method is meant for external methods which need to know the suffix of
  // the current install at run-time, not for install-time decisions.
  // There are no guarantees that this suffix will not change later:
  // (e.g. if two user-level installs were previously installed in parallel on
  // the same machine, both without admin rights and with no user-level install
  // having claimed the non-suffixed HKLM registrations, they both have no
  // suffix in their progId entries (as per the old suffix rules). If they were
  // to both fully register (i.e. click "Make Chrome Default" and go through
  // UAC; or upgrade to Win8 and get the automatic no UAC full registration)
  // they would then both get a suffixed registration as per the new suffix
  // rules).
  //
  // |chrome_exe| The path to the currently installed (or running) chrome.exe.
  static string16 GetCurrentInstallationSuffix(BrowserDistribution* dist,
                                               const string16& chrome_exe);

  // Returns the application name of the program under |dist|.
  // This application name will be suffixed as is appropriate for the current
  // install.
  // This is the name that is registered with Default Programs on Windows and
  // that should thus be used to "make chrome default" and such.
  static string16 GetApplicationName(BrowserDistribution* dist,
                                     const string16& chrome_exe);

  // Returns the AppUserModelId for |dist|. This identifier is unconditionally
  // suffixed with a unique id for this user on user-level installs (in contrast
  // to other registration entries which are suffixed as described in
  // GetCurrentInstallationSuffix() above).
  static string16 GetBrowserModelId(BrowserDistribution* dist,
                                    const string16& chrome_exe);

  // Returns an AppUserModelId composed of each member of |components| separated
  // by dots.
  // The returned appid is guaranteed to be no longer than
  // chrome::kMaxAppModelIdLength (some of the components might have been
  // shortened to enforce this).
  static string16 BuildAppModelId(const std::vector<string16>& components);

  // Returns true if Chrome can make itself the default browser without relying
  // on the Windows shell to prompt the user. This is the case for versions of
  // Windows prior to Windows 8.
  static bool CanMakeChromeDefaultUnattended();

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
                                const string16& chrome_exe,
                                bool elevate_if_not_admin);

  // Shows to the user a system dialog where Chrome can be set as the
  // default browser. This is intended for Windows 8 and above only.
  // This is a blocking call.
  //
  // |dist| gives the type of browser distribution currently in use.
  // |chrome_exe| The chrome.exe path to register as default browser.
  static bool ShowMakeChromeDefaultSystemUI(BrowserDistribution* dist,
                                            const string16& chrome_exe);

  // Make Chrome the default application for a protocol.
  // chrome_exe: The chrome.exe path to register as default browser.
  // protocol: The protocol to register as the default handler for.
  static bool MakeChromeDefaultProtocolClient(BrowserDistribution* dist,
                                              const string16& chrome_exe,
                                              const string16& protocol);

  // Registers Chrome as a potential default browser and handler for filetypes
  // and protocols.
  // If Chrome is already registered, this method is a no-op.
  // This method requires write access to HKLM (prior to Win8) so is just a
  // best effort deal.
  // If write to HKLM is required, but fails, and:
  // - |elevate_if_not_admin| is true (and OS is Vista or above):
  //   tries to launch setup.exe with admin priviledges (by prompting the user
  //   with a UAC) to do these tasks.
  // - |elevate_if_not_admin| is false (or OS is XP):
  //   adds the ProgId entries to HKCU. These entries will not make Chrome show
  //   in Default Programs but they are still useful because Chrome can be
  //   registered to run when the user clicks on an http link or an html file.
  //
  // |chrome_exe| full path to chrome.exe.
  // |unique_suffix| Optional input. If given, this function appends the value
  // to default browser entries names that it creates in the registry.
  // Currently, this is only used to continue an install with the same suffix
  // when elevating and calling setup.exe with admin privileges as described
  // above.
  // |elevate_if_not_admin| if true will make this method try alternate methods
  // as described above. This should only be true when following a user action
  // (e.g. "Make Chrome Default") as it allows this method to UAC.
  //
  // Returns true if Chrome is successfully registered (or already registered).
  static bool RegisterChromeBrowser(BrowserDistribution* dist,
                                    const string16& chrome_exe,
                                    const string16& unique_suffix,
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
                                        const string16& chrome_exe,
                                        const string16& unique_suffix,
                                        const string16& protocol,
                                        bool elevate_if_not_admin);

  // Remove Chrome shortcut from Desktop.
  // If |shell_change| is CURRENT_USER, the shortcut is removed from the
  // Desktop folder of current user's profile.
  // If |shell_change| is SYSTEM_LEVEL, the shortcut is removed from the
  // Desktop folder of "All Users" profile.
  // |options|: bitfield for which the options come from ChromeShortcutOptions.
  // Only SHORTCUT_ALTERNATE is a valid option for this function.
  static bool RemoveChromeDesktopShortcut(BrowserDistribution* dist,
                                          int shell_change,
                                          uint32 options);

  // Removes a set of existing Chrome desktop shortcuts. |appended_names| is a
  // list of shortcut file names as obtained from
  // ShellUtil::GetChromeShortcutName.
  static bool RemoveChromeDesktopShortcutsWithAppendedNames(
      const std::vector<string16>& appended_names);

  // Remove Chrome shortcut from Quick Launch Bar.
  // If shell_change is CURRENT_USER, the shortcut is removed from
  // the Quick Launch folder of current user's profile.
  // If shell_change is SYSTEM_LEVEL, the shortcut is removed from
  // the Quick Launch folder of "Default User" profile.
  static bool RemoveChromeQuickLaunchShortcut(BrowserDistribution* dist,
                                              int shell_change);

  // This will remove all secondary tiles from the start screen for |dist|.
  static void RemoveChromeStartScreenShortcuts(BrowserDistribution* dist,
                                               const string16& chrome_exe);

  enum ChromeShortcutOptions {
    SHORTCUT_NO_OPTIONS = 0,
    // Set DualMode property for Windows 8 Metro-enabled shortcuts.
    SHORTCUT_DUAL_MODE = 1 << 0,
    // Create a new shortcut (overwriting if necessary).
    SHORTCUT_CREATE_ALWAYS = 1 << 1,
    // Use an alternate application name for the shortcut (e.g. "The Internet").
    // This option is only applied to the Desktop shortcut.
    SHORTCUT_ALTERNATE = 1 << 2,
  };

  // Updates shortcut (or creates a new shortcut) at destination given by
  // shortcut to a target given by chrome_exe. The arguments are given by
  // |arguments| for the target and icon is set based on |icon_path| and
  // |icon_index|. If create_new is set to true, the function will create a new
  // shortcut if it doesn't exist.
  // |options|: bitfield for which the options come from ChromeShortcutOptions.
  // If SHORTCUT_CREATE_ALWAYS is not set in |options|, only specified (non-
  // null) properties on an existing shortcut will be modified. If the shortcut
  // does not exist, this method is a no-op and returns false.
  static bool UpdateChromeShortcut(BrowserDistribution* dist,
                                   const string16& chrome_exe,
                                   const string16& shortcut,
                                   const string16& arguments,
                                   const string16& description,
                                   const string16& icon_path,
                                   int icon_index,
                                   uint32 options);

  // Sets |suffix| to the base 32 encoding of the md5 hash of this user's sid
  // preceded by a dot.
  // This is guaranteed to be unique on the machine and 27 characters long
  // (including the '.').
  // This suffix is then meant to be added to all registration that may conflict
  // with another user-level Chrome install.
  // Note that prior to Chrome 21, the suffix registered used to be the user's
  // username (see GetOldUserSpecificRegistrySuffix() below). We still honor old
  // installs registered that way, but it was wrong because some of the
  // characters allowed in a username are not allowed in a ProgId.
  // Returns true unless the OS call to retrieve the username fails.
  // NOTE: Only the installer should use this suffix directly. Other callers
  // should call GetCurrentInstallationSuffix().
  static bool GetUserSpecificRegistrySuffix(string16* suffix);

  // Sets |suffix| to this user's username preceded by a dot. This suffix should
  // only be used to support legacy installs that used this suffixing
  // style.
  // Returns true unless the OS call to retrieve the username fails.
  // NOTE: Only the installer should use this suffix directly. Other callers
  // should call GetCurrentInstallationSuffix().
  static bool GetOldUserSpecificRegistrySuffix(string16* suffix);

  // Returns the base32 encoding (using the [A-Z2-7] alphabet) of |bytes|.
  // |size| is the length of |bytes|.
  // Note: This method does not suffix the output with '=' signs as technically
  // required by the base32 standard for inputs that aren't a multiple of 5
  // bytes.
  static string16 ByteArrayToBase32(const uint8* bytes, size_t size);

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellUtil);
};


#endif  // CHROME_INSTALLER_UTIL_SHELL_UTIL_H_
