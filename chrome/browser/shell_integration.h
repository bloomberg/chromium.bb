// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHELL_INTEGRATION_H_
#define CHROME_BROWSER_SHELL_INTEGRATION_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"

class CommandLine;
class FilePath;

#if defined(USE_X11)
namespace base {
class Environment;
}
#endif

class ShellIntegration {
 public:
  // Sets Chrome as the default browser (only for the current user). Returns
  // false if this operation fails.
  static bool SetAsDefaultBrowser();

  // Sets Chrome as the default client application for the given protocol
  // (only for the current user). Returns false if this operation fails.
  static bool SetAsDefaultProtocolClient(const std::string& protocol);

  // Returns true if the running browser can be set as the default browser.
  static bool CanSetAsDefaultBrowser();

  // Returns true if the running browser can be set as the default client
  // application for specific protocols.
  static bool CanSetAsDefaultProtocolClient();

  // On Linux, it may not be possible to determine or set the default browser
  // on some desktop environments or configurations. So, we use this enum and
  // not a plain bool. (Note however that if used like a bool, this enum will
  // have reasonable behavior.)
  enum DefaultWebClientState {
    NOT_DEFAULT_WEB_CLIENT = 0,
    IS_DEFAULT_WEB_CLIENT,
    UNKNOWN_DEFAULT_WEB_CLIENT = -1
  };

  // Attempt to determine if this instance of Chrome is the default browser and
  // return the appropriate state. (Defined as being the handler for HTTP/HTTPS
  // protocols; we don't want to report "no" here if the user has simply chosen
  // to open HTML files in a text editor and FTP links with an FTP client.)
  static DefaultWebClientState IsDefaultBrowser();

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
    string16 title;
    string16 description;
    SkBitmap favicon;

    bool create_on_desktop;
    bool create_in_applications_menu;

    // For Windows, this refers to quick launch bar prior to Win7. In Win7,
    // this means "pin to taskbar". For Mac/Linux, this could be used for
    // Mac dock or the gnome/kde application launcher. However, those are not
    // implemented yet.
    bool create_in_quick_launch_bar;
  };

  // Set up command line arguments for launching a URL or an app.
  // The new command line reuses the current process's user data directory (and
  // login profile, for ChromeOS).
  // If |extension_app_id| is non-empty, the arguments use kAppId=<id>.
  // Otherwise, kApp=<url> is used.
  static CommandLine CommandLineArgsForLauncher(
      const GURL& url,
      const std::string& extension_app_id);

#if defined(USE_X11)
  // Returns filename of the desktop shortcut used to launch the browser.
  static std::string GetDesktopName(base::Environment* env);

  static bool GetDesktopShortcutTemplate(base::Environment* env,
                                         std::string* output);

  // Returns filename for .desktop file based on |url|, sanitized for security.
  static FilePath GetDesktopShortcutFilename(const GURL& url);

  // Returns contents for .desktop file based on |template_contents|, |url|
  // and |title|. The |template_contents| should be contents of .desktop file
  // used to launch Chrome.
  static std::string GetDesktopFileContents(
      const std::string& template_contents,
      const std::string& app_name,
      const GURL& url,
      const std::string& extension_id,
      const string16& title,
      const std::string& icon_name);

  static void CreateDesktopShortcut(const ShortcutInfo& shortcut_info,
                                    const std::string& shortcut_template);
#endif  // defined(USE_X11)

#if defined(OS_WIN)
  // Generates Win7 app id for given app name and profile path. The returned app
  // id is in the format of "|app_name|[.<profile_id>]". "profile_id" is
  // appended when user override the default value.
  static std::wstring GetAppId(const std::wstring& app_name,
                               const FilePath& profile_path);

  // Generates Win7 app id for Chromium by calling GetAppId with
  // chrome::kBrowserAppID as app_name.
  static std::wstring GetChromiumAppId(const FilePath& profile_path);

  // Returns the path to the Chromium icon. This is used to specify the icon
  // to use for the taskbar group on Win 7.
  static string16 GetChromiumIconPath();

  // Migrates existing chrome shortcuts by tagging them with correct app id.
  // see http://crbug.com/28104
  static void MigrateChromiumShortcuts();
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
    // Observer classes that return true to OwnedByWorker are automatically
    // freed by the worker when they are no longer needed.
    virtual bool IsOwnedByWorker() { return false; }
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

    // Function that sets Chrome as the default web client.
    virtual void SetAsDefault() = 0;

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
    void ExecuteSetAsDefault();

    // Communicate results to the observer. This function is posted as a task
    // onto the UI thread by the ExecuteCheckIsDefault function running in the
    // file thread.
    void CompleteCheckIsDefault(DefaultWebClientState state);

    // When the action to set Chrome as the default has completed this function
    // is run. It is posted as a task back onto the UI thread by the
    // ExecuteSetAsDefault function running in the file thread. This function
    // will the start the check process, which, if an observer is present,
    // reports to it the new status.
    void CompleteSetAsDefault();

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
    virtual DefaultWebClientState CheckIsDefault();

    // Set Chrome as the default browser.
    virtual void SetAsDefault();

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
    virtual DefaultWebClientState CheckIsDefault();

    // Set Chrome as the default handler for this protocol.
    virtual void SetAsDefault();

    std::string protocol_;

    DISALLOW_COPY_AND_ASSIGN(DefaultProtocolClientWorker);
  };
};

#endif  // CHROME_BROWSER_SHELL_INTEGRATION_H_
