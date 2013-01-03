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
#include "base/file_path.h"
#include "base/logging.h"
#include "base/string16.h"
#include "chrome/installer/util/work_item_list.h"

class BrowserDistribution;

// This is a utility class that provides common shell integration methods
// that can be used by installer as well as Chrome.
class ShellUtil {
 public:
  // Input to any methods that make changes to OS shell.
  enum ShellChange {
    CURRENT_USER = 0x1,  // Make any shell changes only at the user level
    SYSTEM_LEVEL = 0x2   // Make any shell changes only at the system level
  };

  // Chrome's default handler state for a given protocol.
  enum DefaultState {
    UNKNOWN_DEFAULT,
    NOT_DEFAULT,
    IS_DEFAULT,
  };

  // Typical shortcut directories. Resolved in GetShortcutPath().
  enum ShortcutLocation {
    SHORTCUT_LOCATION_DESKTOP,
    SHORTCUT_LOCATION_QUICK_LAUNCH,
    SHORTCUT_LOCATION_START_MENU,
  };

  enum ShortcutOperation {
    // Create a new shortcut (overwriting if necessary).
    SHELL_SHORTCUT_CREATE_ALWAYS,
    // Create the per-user shortcut only if its system-level equivalent is not
    // present.
    SHELL_SHORTCUT_CREATE_IF_NO_SYSTEM_LEVEL,
    // Overwrite an existing shortcut (fail if the shortcut doesn't exist).
    // If the arguments are not specified on the new shortcut, keep the old
    // shortcut's arguments.
    SHELL_SHORTCUT_REPLACE_EXISTING,
    // Update specified properties only on an existing shortcut.
    SHELL_SHORTCUT_UPDATE_EXISTING,
  };

  // Properties for shortcuts. Properties set will be applied to
  // the shortcut on creation/update. On update, unset properties are ignored;
  // on create (and replaced) unset properties might have a default value (see
  // individual property setters below for details).
  // Callers are encouraged to use the setters provided which take care of
  // setting |options| as desired.
  struct ShortcutProperties {
    enum IndividualProperties {
      PROPERTIES_TARGET = 1 << 0,
      PROPERTIES_ARGUMENTS = 1 << 1,
      PROPERTIES_DESCRIPTION = 1 << 2,
      PROPERTIES_ICON = 1 << 3,
      PROPERTIES_APP_ID = 1 << 4,
      PROPERTIES_SHORTCUT_NAME = 1 << 5,
      PROPERTIES_DUAL_MODE = 1 << 6,
    };

    explicit ShortcutProperties(ShellChange level_in)
        : level(level_in), icon_index(0), dual_mode(false),
          pin_to_taskbar(false), options(0U) {}

    // Sets the target executable to launch from this shortcut.
    // This is mandatory when creating a shortcut.
    void set_target(const FilePath& target_in) {
      target = target_in;
      options |= PROPERTIES_TARGET;
    }

    // Sets the arguments to be passed to |target| when launching from this
    // shortcut.
    // The length of this string must be less than MAX_PATH.
    void set_arguments(const string16& arguments_in) {
      // Size restriction as per MSDN at
      // http://msdn.microsoft.com/library/windows/desktop/bb774954.aspx.
      DCHECK(arguments_in.length() < MAX_PATH);
      arguments = arguments_in;
      options |= PROPERTIES_ARGUMENTS;
    }

    // Sets the localized description of the shortcut.
    // The length of this string must be less than MAX_PATH.
    void set_description(const string16& description_in) {
      // Size restriction as per MSDN at
      // http://msdn.microsoft.com/library/windows/desktop/bb774955.aspx.
      DCHECK(description_in.length() < MAX_PATH);
      description = description_in;
      options |= PROPERTIES_DESCRIPTION;
    }

    // Sets the path to the icon (icon_index set to 0).
    // icon index unless otherwise specified in master_preferences).
    void set_icon(const FilePath& icon_in, int icon_index_in) {
      icon = icon_in;
      icon_index = icon_index_in;
      options |= PROPERTIES_ICON;
    }

    // Sets the app model id for the shortcut (Win7+).
    void set_app_id(const string16& app_id_in) {
      app_id = app_id_in;
      options |= PROPERTIES_APP_ID;
    }

    // Forces the shortcut's name to |shortcut_name_in|.
    // Default: the current distribution's GetAppShortcutName().
    // The ".lnk" extension will automatically be added to this name.
    void set_shortcut_name(const string16& shortcut_name_in) {
      shortcut_name = shortcut_name_in;
      options |= PROPERTIES_SHORTCUT_NAME;
    }

    // Sets whether this is a dual mode shortcut (Win8+).
    // NOTE: Only the default (no arguments and default browser appid) browser
    // shortcut in the Start menu (Start screen on Win8+) should be made dual
    // mode.
    void set_dual_mode(bool dual_mode_in) {
      dual_mode = dual_mode_in;
      options |= PROPERTIES_DUAL_MODE;
    }

    // Sets whether to pin this shortcut to the taskbar after creating it
    // (ignored if the shortcut is only being updated).
    // Note: This property doesn't have a mask in |options|.
    void set_pin_to_taskbar(bool pin_to_taskbar_in) {
      pin_to_taskbar = pin_to_taskbar_in;
    }

    bool has_target() const {
      return (options & PROPERTIES_TARGET) != 0;
    }

    bool has_arguments() const {
      return (options & PROPERTIES_ARGUMENTS) != 0;
    }

    bool has_description() const {
      return (options & PROPERTIES_DESCRIPTION) != 0;
    }

    bool has_icon() const {
      return (options & PROPERTIES_ICON) != 0;
    }

    bool has_app_id() const {
      return (options & PROPERTIES_APP_ID) != 0;
    }

    bool has_shortcut_name() const {
      return (options & PROPERTIES_SHORTCUT_NAME) != 0;
    }

    bool has_dual_mode() const {
      return (options & PROPERTIES_DUAL_MODE) != 0;
    }

    // The level to install this shortcut at (CURRENT_USER for a per-user
    // shortcut and SYSTEM_LEVEL for an all-users shortcut).
    ShellChange level;

    FilePath target;
    string16 arguments;
    string16 description;
    FilePath icon;
    int icon_index;
    string16 app_id;
    string16 shortcut_name;
    bool dual_mode;
    bool pin_to_taskbar;
    // Bitfield made of IndividualProperties. Properties set in |options| will
    // be used to create/update the shortcut, others will be ignored on update
    // and possibly replaced by default values on create (see individual
    // property setters above for details on default values).
    uint32 options;
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

  // Sets |path| to the path for a shortcut at the |location| desired for the
  // given |level| (CURRENT_USER for per-user path and SYSTEM_LEVEL for
  // all-users path).
  // Returns false on failure.
  static bool GetShortcutPath(ShellUtil::ShortcutLocation location,
                              BrowserDistribution* dist,
                              ShellChange level,
                              FilePath* path);

  // Updates shortcut in |location| (or creates it if |options| specify
  // SHELL_SHORTCUT_CREATE_ALWAYS).
  // |dist| gives the type of browser distribution currently in use.
  // |properties| and |operation| affect this method as described on their
  // invidividual definitions above.
  static bool CreateOrUpdateShortcut(
      ShellUtil::ShortcutLocation location,
      BrowserDistribution* dist,
      const ShellUtil::ShortcutProperties& properties,
      ShellUtil::ShortcutOperation operation);

  // Returns the string "|icon_path|,|icon_index|" (see, for example,
  // http://msdn.microsoft.com/library/windows/desktop/dd391573.aspx).
  static string16 FormatIconLocation(const string16& icon_path, int icon_index);

  // This method returns the command to open URLs/files using chrome. Typically
  // this command is written to the registry under shell\open\command key.
  // |chrome_exe|: the full path to chrome.exe
  static string16 GetChromeShellOpenCmd(const string16& chrome_exe);

  // This method returns the command to be called by the DelegateExecute verb
  // handler to launch chrome on Windows 8. Typically this command is written to
  // the registry under the HKCR\Chrome\.exe\shell\(open|run)\command key.
  // |chrome_exe|: the full path to chrome.exe
  static string16 GetChromeDelegateCommand(const string16& chrome_exe);

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
                                    bool is_per_user_install);

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

  // Returns the DefaultState of Chrome for HTTP and HTTPS.
  static DefaultState GetChromeDefaultState();

  // Returns the DefaultState of Chrome for |protocol|.
  static DefaultState GetChromeDefaultProtocolClientState(
      const string16& protocol);

  // Make Chrome the default browser. This function works by going through
  // the url protocols and file associations that are related to general
  // browsing, e.g. http, https, .html etc., and requesting to become the
  // default handler for each. If any of these fails the operation will return
  // false to indicate failure, which is consistent with the return value of
  // ShellIntegration::GetDefaultBrowser.
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

  // Shows and waits for the Windows 8 "How do you want to open webpages?"
  // dialog if Chrome is not already the default HTTP/HTTPS handler. Also does
  // XP-era registrations if Chrome is chosen or was already the default. Do
  // not use on pre-Win8 OSes.
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

  // Shows and waits for the Windows 8 "How do you want to open links of this
  // type?" dialog if Chrome is not already the default |protocol|
  // handler. Also does XP-era registrations if Chrome is chosen or was already
  // the default for |protocol|. Do not use on pre-Win8 OSes.
  //
  // |dist| gives the type of browser distribution currently in use.
  // |chrome_exe| The chrome.exe path to register as default browser.
  // |protocol| is the protocol being registered.
  static bool ShowMakeChromeDefaultProtocolClientSystemUI(
      BrowserDistribution* dist,
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

  // Removes installed shortcut at |location|.
  // |chrome_exe|: The path to the chrome.exe being uninstalled; the shortcut
  // will only be deleted if its target is also |chrome_exe|.
  // |level|: CURRENT_USER to remove the per-user shortcut and SYSTEM_LEVEL to
  // remove the all-users shortcut.
  // |shortcut_name|: If non-null, remove the shortcut named |shortcut_name| at
  // location; otherwise remove the default shortcut at |location|.
  // If |location| is SHORTCUT_LOCATION_START_MENU the shortcut folder specific
  // to |dist| is deleted.
  // Also attempts to unpin the removed shortcut from the taskbar.
  // Returns true if the shortcut was successfully deleted (or there is no
  // shortcut at |location| pointing to |chrome_exe|).
  static bool RemoveShortcut(ShellUtil::ShortcutLocation location,
                             BrowserDistribution* dist,
                             const string16& target_exe,
                             ShellChange level,
                             const string16* shortcut_name);

  // Enumerates all shortcuts pinned to the taskbar and deletes those pointing
  // to |target_exe|.
  // base::win::TaskbarUnpinShortcutLink() should be prefered, but this is
  // useful on uninstall as the parent shortcut of a pin might no longer exist
  // (thus making it impossible to unpin it via that API).
  static void RemoveTaskbarShortcuts(const string16& target_exe);

  // This will remove all secondary tiles from the start screen for |dist|.
  static void RemoveStartScreenShortcuts(BrowserDistribution* dist,
                                         const string16& target_exe);

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
