// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHELL_INTEGRATION_H_
#define CHROME_BROWSER_SHELL_INTEGRATION_H_

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/image/image.h"

class CommandLine;

class ShellIntegration {
 public:
  // Sets Chrome as the default browser (only for the current user). Returns
  // false if this operation fails.
  static bool SetAsDefaultBrowser();

  // Initiates an OS shell flow which (if followed by the user) should set
  // Chrome as the default browser. Returns false if the flow cannot be
  // initialized, if it is not supported (introduced for Windows 8) or if the
  // user cancels the operation. This is a blocking call and requires a FILE
  // thread. If Chrome is already default browser, no interactive dialog will be
  // shown and this method returns true.
  static bool SetAsDefaultBrowserInteractive();

  // Sets Chrome as the default client application for the given protocol
  // (only for the current user). Returns false if this operation fails.
  static bool SetAsDefaultProtocolClient(const std::string& protocol);

  // Initiates an OS shell flow which (if followed by the user) should set
  // Chrome as the default handler for |protocol|. Returns false if the flow
  // cannot be initialized, if it is not supported (introduced for Windows 8)
  // or if the user cancels the operation. This is a blocking call and requires
  // a FILE thread. If Chrome is already default for |protocol|, no interactive
  // dialog will be shown and this method returns true.
  static bool SetAsDefaultProtocolClientInteractive(
      const std::string& protocol);

  // In Windows 8 a browser can be made default-in-metro only in an interactive
  // flow. We will distinguish between two types of permissions here to avoid
  // forcing the user into UI interaction when this should not be done.
  enum DefaultWebClientSetPermission {
    SET_DEFAULT_NOT_ALLOWED,
    SET_DEFAULT_UNATTENDED,
    SET_DEFAULT_INTERACTIVE,
  };

  // Returns requirements for making the running browser the user's default.
  static DefaultWebClientSetPermission CanSetAsDefaultBrowser();

  // Returns requirements for making the running browser the user's default
  // client application for specific protocols.
  static DefaultWebClientSetPermission CanSetAsDefaultProtocolClient();

  // On Linux, it may not be possible to determine or set the default browser
  // on some desktop environments or configurations. So, we use this enum and
  // not a plain bool.
  enum DefaultWebClientState {
    NOT_DEFAULT,
    IS_DEFAULT,
    UNKNOWN_DEFAULT,
    NUM_DEFAULT_STATES
  };

  // Attempt to determine if this instance of Chrome is the default browser and
  // return the appropriate state. (Defined as being the handler for HTTP/HTTPS
  // protocols; we don't want to report "no" here if the user has simply chosen
  // to open HTML files in a text editor and FTP links with an FTP client.)
  static DefaultWebClientState GetDefaultBrowser();

  // Returns true if Firefox is likely to be the default browser for the current
  // user. This method is very fast so it can be invoked in the UI thread.
  static bool IsFirefoxDefaultBrowser();

  // Attempt to determine if this instance of Chrome is the default client
  // application for the given protocol and return the appropriate state.
  static DefaultWebClientState
      IsDefaultProtocolClient(const std::string& protocol);

  struct ShortcutInfo {
    ShortcutInfo();
    ~ShortcutInfo();

    GURL url;
    // If |extension_id| is non-empty, this is short cut is to an extension-app
    // and the launch url will be detected at start-up. In this case, |url|
    // is still used to generate the app id (windows app id, not chrome app id).
    std::string extension_id;
    bool is_platform_app;
    string16 title;
    string16 description;
    FilePath extension_path;
    gfx::Image favicon;
    FilePath profile_path;

    bool create_on_desktop;
    bool create_in_applications_menu;

    // For Windows, this refers to quick launch bar prior to Win7. In Win7,
    // this means "pin to taskbar". For Mac/Linux, this could be used for
    // Mac dock or the gnome/kde application launcher. However, those are not
    // implemented yet.
    bool create_in_quick_launch_bar;
  };

  // Data that needs to be passed between the app launcher stub and Chrome.
  struct AppModeInfo {
  };
  static void SetAppModeInfo(const AppModeInfo* info);
  static const AppModeInfo* AppModeInfo();

  // Is the current instance of Chrome running in App mode.
  bool IsRunningInAppMode();

  // Set up command line arguments for launching a URL or an app.
  // The new command line reuses the current process's user data directory (and
  // login profile, for ChromeOS).
  // If |extension_app_id| is non-empty, the arguments use kAppId=<id>.
  // Otherwise, kApp=<url> is used.
  static CommandLine CommandLineArgsForLauncher(
      const GURL& url,
      const std::string& extension_app_id,
      const FilePath& profile_path);

#if defined(OS_WIN)
  // Generates an application user model ID (AppUserModelId) for a given app
  // name and profile path. The returned app id is in the format of
  // "|app_name|[.<profile_id>]". "profile_id" is appended when user override
  // the default value.
  // Note: If the app has an installation specific suffix (e.g. on user-level
  // Chrome installs), |app_name| should already be suffixed, this method will
  // then further suffix it with the profile id as described above.
  static string16 GetAppModelIdForProfile(const string16& app_name,
                                          const FilePath& profile_path);

  // Generates an application user model ID (AppUserModelId) for Chromium by
  // calling GetAppModelIdForProfile() with ShellUtil::GetAppId() as app_name.
  static string16 GetChromiumModelIdForProfile(const FilePath& profile_path);

  // Get the AppUserModelId for the App List, for the profile in |profile_path|.
  static string16 GetAppListAppModelIdForProfile(const FilePath& profile_path);

  // Returns the location (path and index) of the Chromium icon, (e.g.,
  // "C:\path\to\chrome.exe,0"). This is used to specify the icon to use
  // for the taskbar group on Win 7.
  static string16 GetChromiumIconLocation();

  // Migrates existing chrome shortcuts by tagging them with correct app id.
  // see http://crbug.com/28104
  static void MigrateChromiumShortcuts();

  // Migrates all shortcuts in |path| which point to |chrome_exe| such that they
  // have the appropriate AppUserModelId. Also makes sure those shortcuts have
  // the dual_mode property set if such is requested by |check_dual_mode|.
  // Returns the number of shortcuts migrated.
  // This method should not be called prior to Windows 7.
  // This method is only public for the sake of tests and shouldn't be called
  // externally otherwise.
  static int MigrateShortcutsInPathInternal(const FilePath& chrome_exe,
                                            const FilePath& path,
                                            bool check_dual_mode);

  // Returns the path to the Start Menu shortcut for the given Chrome.
  static FilePath GetStartMenuShortcut(const FilePath& chrome_exe);
#endif  // defined(OS_WIN)

  // The current default web client application UI state. This is used when
  // querying if Chrome is the default browser or the default handler
  // application for a url protocol, and communicates the state and result of
  // a request.
  enum DefaultWebClientUIState {
    STATE_PROCESSING,
    STATE_NOT_DEFAULT,
    STATE_IS_DEFAULT,
    STATE_UNKNOWN
  };

  class DefaultWebClientObserver {
   public:
    virtual ~DefaultWebClientObserver() {}
    // Updates the UI state to reflect the current default browser state.
    virtual void SetDefaultWebClientUIState(DefaultWebClientUIState state) = 0;
    // Called to notify the UI of the immediate result of invoking
    // SetAsDefault.
    virtual void OnSetAsDefaultConcluded(bool succeeded) {}
    // Observer classes that return true to OwnedByWorker are automatically
    // freed by the worker when they are no longer needed. False by default.
    virtual bool IsOwnedByWorker();
    // An observer can permit or decline set-as-default operation if it
    // requires triggering user interaction. By default not allowed.
    virtual bool IsInteractiveSetDefaultPermitted();
  };

  //  Helper objects that handle checking if Chrome is the default browser
  //  or application for a url protocol on Windows and Linux, and also setting
  //  it as the default. These operations are performed asynchronously on the
  //  file thread since registry access (on Windows) or the preference database
  //  (on Linux) are involved and this can be slow.
  class DefaultWebClientWorker
      : public base::RefCountedThreadSafe<DefaultWebClientWorker> {
   public:
    explicit DefaultWebClientWorker(DefaultWebClientObserver* observer);

    // Checks to see if Chrome is the default web client application. The result
    // will be passed back to the observer via the SetDefaultWebClientUIState
    // function. If there is no observer, this function does not do anything.
    void StartCheckIsDefault();

    // Sets Chrome as the default web client application. If there is an
    // observer, once the operation has completed the new default will be
    // queried and the current status reported via SetDefaultWebClientUIState.
    void StartSetAsDefault();

    // Called to notify the worker that the view is gone.
    void ObserverDestroyed();

   protected:
    friend class base::RefCountedThreadSafe<DefaultWebClientWorker>;

    virtual ~DefaultWebClientWorker() {}

   private:
    // Function that performs the check.
    virtual DefaultWebClientState CheckIsDefault() = 0;

    // Function that sets Chrome as the default web client. Returns false if
    // the operation fails or has been cancelled by the user.
    virtual bool SetAsDefault(bool interactive_permitted) = 0;

    // Function that handles performing the check on the file thread. This
    // function is posted as a task onto the file thread, where it performs
    // the check. When the check has finished the CompleteCheckIsDefault
    // function is posted to the UI thread, where the result is sent back to
    // the observer.
    void ExecuteCheckIsDefault();

    // Function that handles setting Chrome as the default web client on the
    // file thread. This function is posted as a task onto the file thread.
    // Once it is finished the CompleteSetAsDefault function is posted to the
    // UI thread which will check the status of Chrome as the default, and
    // send this to the observer.
    // |interactive_permitted| indicates if the routine is allowed to carry on
    // in context where user interaction is required (CanSetAsDefault*
    // returns SET_DEFAULT_INTERACTIVE).
    void ExecuteSetAsDefault(bool interactive_permitted);

    // Communicate results to the observer. This function is posted as a task
    // onto the UI thread by the ExecuteCheckIsDefault function running in the
    // file thread.
    void CompleteCheckIsDefault(DefaultWebClientState state);

    // When the action to set Chrome as the default has completed this function
    // is run. It is posted as a task back onto the UI thread by the
    // ExecuteSetAsDefault function running in the file thread. This function
    // will the start the check process, which, if an observer is present,
    // reports to it the new status.
    // |succeeded| is true if the actual call to a set-default function (from
    // ExecuteSetAsDefault) was successful.
    void CompleteSetAsDefault(bool succeeded);

    // Updates the UI in our associated view with the current default web
    // client state.
    void UpdateUI(DefaultWebClientState state);

    DefaultWebClientObserver* observer_;

    DISALLOW_COPY_AND_ASSIGN(DefaultWebClientWorker);
  };

  // Worker for checking and setting the default browser.
  class DefaultBrowserWorker : public DefaultWebClientWorker {
   public:
    explicit DefaultBrowserWorker(DefaultWebClientObserver* observer);

   private:
    virtual ~DefaultBrowserWorker() {}

    // Check if Chrome is the default browser.
    virtual DefaultWebClientState CheckIsDefault() OVERRIDE;

    // Set Chrome as the default browser.
    virtual bool SetAsDefault(bool interactive_permitted) OVERRIDE;

    DISALLOW_COPY_AND_ASSIGN(DefaultBrowserWorker);
  };

  // Worker for checking and setting the default client application
  // for a given protocol. A different worker instance is needed for each
  // protocol you are interested in, so to check or set the default for
  // multiple protocols you should use multiple worker objects.
  class DefaultProtocolClientWorker : public DefaultWebClientWorker {
   public:
    DefaultProtocolClientWorker(DefaultWebClientObserver* observer,
                                const std::string& protocol);

    const std::string& protocol() const { return protocol_; }

   protected:
    virtual ~DefaultProtocolClientWorker() {}

   private:
    // Check is Chrome is the default handler for this protocol.
    virtual DefaultWebClientState CheckIsDefault() OVERRIDE;

    // Set Chrome as the default handler for this protocol.
    virtual bool SetAsDefault(bool interactive_permitted) OVERRIDE;

    std::string protocol_;

    DISALLOW_COPY_AND_ASSIGN(DefaultProtocolClientWorker);
  };
};

#endif  // CHROME_BROWSER_SHELL_INTEGRATION_H_
