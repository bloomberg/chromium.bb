// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHELL_INTEGRATION_H_
#define CHROME_BROWSER_SHELL_INTEGRATION_H_

#include <string>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "ui/gfx/image/image_family.h"
#include "url/gurl.h"

namespace base {
class CommandLine;
class OneShotTimer;
}

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

  // Returns true if setting the default browser is an asynchronous operation.
  // In practice, this is only true on Windows 10+.
  static bool IsSetAsDefaultAsynchronous();

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

  // Windows 8 and Windows 10 introduced different ways to set the default
  // browser.
  enum DefaultWebClientSetPermission {
    // The browser distribution is not permitted to be made default.
    SET_DEFAULT_NOT_ALLOWED,
    // No special permission or interaction is required to set the default
    // browser. This is used in Linux, Mac and Windows 7 and under.
    SET_DEFAULT_UNATTENDED,
    // On Windows 8, a browser can be made default only in an interactive flow.
    SET_DEFAULT_INTERACTIVE,
    // On Windows 10+, the set default browser flow is both interactive and
    // asynchronous.
    SET_DEFAULT_ASYNCHRONOUS,
  };

  // Returns requirements for making the running browser the user's default.
  static DefaultWebClientSetPermission CanSetAsDefaultBrowser();

  // Returns requirements for making the running browser the user's default
  // client application for specific protocols.
  static DefaultWebClientSetPermission CanSetAsDefaultProtocolClient();

  // Returns true if making the running browser the default client for any
  // protocol requires elevated privileges.
  static bool IsElevationNeededForSettingDefaultProtocolClient();

  // Returns a string representing the application to be launched given the
  // protocol of the requested url. This string may be a name or a path, but
  // neither is guaranteed and it should only be used as a display string.
  // Returns an empty string on failure.
  static base::string16 GetApplicationNameForProtocol(const GURL& url);

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
  static base::CommandLine CommandLineArgsForLauncher(
      const GURL& url,
      const std::string& extension_app_id,
      const base::FilePath& profile_path);

  // Append command line arguments for launching a new chrome.exe process
  // based on the current process.
  // The new command line reuses the current process's user data directory and
  // profile.
  static void AppendProfileArgs(const base::FilePath& profile_path,
                                base::CommandLine* command_line);

#if defined(OS_WIN)
  // Generates an application user model ID (AppUserModelId) for a given app
  // name and profile path. The returned app id is in the format of
  // "|app_name|[.<profile_id>]". "profile_id" is appended when user override
  // the default value.
  // Note: If the app has an installation specific suffix (e.g. on user-level
  // Chrome installs), |app_name| should already be suffixed, this method will
  // then further suffix it with the profile id as described above.
  static base::string16 GetAppModelIdForProfile(const base::string16& app_name,
                                          const base::FilePath& profile_path);

  // Generates an application user model ID (AppUserModelId) for Chromium by
  // calling GetAppModelIdForProfile() with ShellUtil::GetAppId() as app_name.
  static base::string16 GetChromiumModelIdForProfile(
      const base::FilePath& profile_path);

  // Get the AppUserModelId for the App List, for the profile in |profile_path|.
  static base::string16 GetAppListAppModelIdForProfile(
      const base::FilePath& profile_path);

  // Migrates existing chrome shortcuts by tagging them with correct app id.
  // see http://crbug.com/28104
  static void MigrateChromiumShortcuts();

  // Migrates all shortcuts in |path| which point to |chrome_exe| such that they
  // have the appropriate AppUserModelId. Also makes sure those shortcuts have
  // the dual_mode (ref. shell_util.h) property set if such is requested by
  // |check_dual_mode|.
  // Returns the number of shortcuts migrated.
  // This method should not be called prior to Windows 7.
  // This method is only public for the sake of tests and shouldn't be called
  // externally otherwise.
  static int MigrateShortcutsInPathInternal(const base::FilePath& chrome_exe,
                                            const base::FilePath& path,
                                            bool check_dual_mode);

  // Returns the path to the Start Menu shortcut for the given Chrome.
  static base::FilePath GetStartMenuShortcut(const base::FilePath& chrome_exe);
#endif  // defined(OS_WIN)

#if !defined(OS_WIN)
  // TODO(calamity): replace with
  // BrowserDistribution::GetStartMenuShortcutSubfolder() once
  // BrowserDistribution is cross-platform.
  // Gets the name of the Chrome Apps menu folder in which to place app
  // shortcuts. This is needed for Mac and Linux.
  static base::string16 GetAppShortcutsSubdirName();
#endif

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

    // Possible result codes for a set-as-default operation.
    // Do not modify the ordering as it is important for UMA.
    enum AttemptResult {
      // No errors encountered.
      SUCCESS,
      // Chrome was already the default web client. This counts as a successful
      // attempt.
      ALREADY_DEFAULT,
      // Chrome was not set as the default web client.
      FAILURE,
      // The attempt was abandoned because the observer was destroyed.
      ABANDONED,
      // Failed to launch the process to set Chrome as the default web client
      // asynchronously.
      LAUNCH_FAILURE,
      // Another worker is already in progress to make Chrome the default web
      // client.
      OTHER_WORKER,
      // The user initiated another attempt while the asynchronous operation was
      // already in progress.
      RETRY,
      NUM_ATTEMPT_RESULT_TYPES
    };

    virtual ~DefaultWebClientWorker();

    // Communicates the result to the observer. In contrast to
    // OnSetAsDefaultAttemptComplete(), this should not be called multiple
    // times.
    void OnCheckIsDefaultComplete(DefaultWebClientState state);

    // Called when the set as default operation is completed. This then invokes
    // FinalizeSetAsDefault() and, if an observer is present, starts the check
    // is default process.
    // It is safe to call this multiple times. Only the first call is processed
    // after StartSetAsDefault() is invoked.
    void OnSetAsDefaultAttemptComplete(AttemptResult result);

    // Returns true if FinalizeSetAsDefault() will be called.
    bool set_as_default_initialized() const {
      return set_as_default_initialized_;
    }

    // Flag that indicates if the set-as-default operation is in progess to
    // prevent multiple notifications to the observer.
    bool set_as_default_in_progress_ = false;

   private:
    // Checks whether Chrome is the default web client. Always called on the
    // FILE thread. Subclasses are responsible for calling
    // OnCheckIsDefaultComplete() on the UI thread.
    virtual void CheckIsDefault() = 0;

    // Sets Chrome as the default web client. Always called on the FILE thread.
    // |interactive_permitted| will make SetAsDefault() fail if it requires
    // interaction with the user. Subclasses are responsible for calling
    // OnSetAsDefaultAttemptComplete() on the UI thread.
    virtual void SetAsDefault(bool interactive_permitted) = 0;

    // Invoked on the UI thread prior to starting a set-as-default operation.
    // Returns true if the initialization succeeded and a subsequent call to
    // FinalizeSetAsDefault() is required.
    virtual bool InitializeSetAsDefault();

    // Invoked on the UI thread following a set-as-default operation.
    virtual void FinalizeSetAsDefault();

    // Returns true if the attempt results should be reported to UMA.
    static bool ShouldReportAttemptResults();

    // Reports the result and duration for one set-as-default attempt.
    void ReportAttemptResult(AttemptResult result);

    // Updates the UI in our associated view with the current default web
    // client state.
    void UpdateUI(DefaultWebClientState state);

    DefaultWebClientObserver* observer_;

    // Flag that indicates the return value of InitializeSetAsDefault(). If
    // true, FinalizeSetAsDefault() will be called to clear what was
    // initialized.
    bool set_as_default_initialized_ = false;

    // Records the time it takes to set the default browser.
    base::TimeTicks start_time_;

    DISALLOW_COPY_AND_ASSIGN(DefaultWebClientWorker);
  };

  // Worker for checking and setting the default browser.
  class DefaultBrowserWorker : public DefaultWebClientWorker {
   public:
    explicit DefaultBrowserWorker(DefaultWebClientObserver* observer);

   private:
    ~DefaultBrowserWorker() override;

    // Check if Chrome is the default browser.
    void CheckIsDefault() override;

    // Set Chrome as the default browser.
    void SetAsDefault(bool interactive_permitted) override;

#if defined(OS_WIN)
    // On Windows 10+, adds the default browser callback and starts the timer
    // that determines if the operation was successful or not.
    bool InitializeSetAsDefault() override;

    // On Windows 10+, removes the default browser callback and stops the timer.
    void FinalizeSetAsDefault() override;

    // Prompts the user to select the default browser by trying to open the help
    // page that explains how to set Chrome as the default browser. Returns
    // false if the process needed to set Chrome default failed to launch.
    static bool SetAsDefaultBrowserAsynchronous();

    // Used to determine if setting the default browser was unsuccesful.
    scoped_ptr<base::OneShotTimer> async_timer_;
#endif  // !defined(OS_WIN)

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
    ~DefaultProtocolClientWorker() override;

   private:
    // Check is Chrome is the default handler for this protocol.
    void CheckIsDefault() override;

    // Set Chrome as the default handler for this protocol.
    void SetAsDefault(bool interactive_permitted) override;

    std::string protocol_;

    DISALLOW_COPY_AND_ASSIGN(DefaultProtocolClientWorker);
  };
};

#endif  // CHROME_BROWSER_SHELL_INTEGRATION_H_
