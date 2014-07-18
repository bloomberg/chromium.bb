// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_EPHEMERAL_APP_LAUNCHER_H_
#define CHROME_BROWSER_APPS_EPHEMERAL_APP_LAUNCHER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/webstore_standalone_installer.h"
#include "chrome/browser/ui/extensions/extension_enable_flow_delegate.h"
#include "content/public/browser/web_contents_observer.h"

class ExtensionEnableFlow;
class Profile;

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
class ExtensionInstallChecker;
class ExtensionRegistry;
}

// EphemeralAppLauncher manages the launching of ephemeral apps. It handles
// display of a prompt, initiates install of the app (if necessary) and finally
// launches the app.
class EphemeralAppLauncher : public extensions::WebstoreStandaloneInstaller,
                             public content::WebContentsObserver,
                             public ExtensionEnableFlowDelegate {
 public:
  typedef base::Callback<void(extensions::webstore_install::Result result,
                              const std::string& error)> LaunchCallback;

  // Returns true if launching ephemeral apps is enabled.
  static bool IsFeatureEnabled();

  // Create for the app launcher.
  static scoped_refptr<EphemeralAppLauncher> CreateForLauncher(
      const std::string& webstore_item_id,
      Profile* profile,
      gfx::NativeWindow parent_window,
      const LaunchCallback& callback);

  // Create for a web contents.
  static scoped_refptr<EphemeralAppLauncher> CreateForWebContents(
      const std::string& webstore_item_id,
      content::WebContents* web_contents,
      const LaunchCallback& callback);

  // Initiate app launch.
  void Start();

 protected:
  EphemeralAppLauncher(const std::string& webstore_item_id,
                       Profile* profile,
                       gfx::NativeWindow parent_window,
                       const LaunchCallback& callback);
  EphemeralAppLauncher(const std::string& webstore_item_id,
                       content::WebContents* web_contents,
                       const LaunchCallback& callback);

  virtual ~EphemeralAppLauncher();

  // Creates an install checker. Allows tests to mock the install checker.
  virtual scoped_ptr<extensions::ExtensionInstallChecker>
      CreateInstallChecker();

  // WebstoreStandaloneInstaller implementation overridden in tests.
  virtual scoped_ptr<ExtensionInstallPrompt> CreateInstallUI() OVERRIDE;
  virtual scoped_ptr<extensions::WebstoreInstaller::Approval> CreateApproval()
      const OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<EphemeralAppLauncher>;
  friend class EphemeralAppLauncherTest;

  // Returns true if an app that is already installed in extension system can
  // be launched.
  bool CanLaunchInstalledApp(const extensions::Extension* extension,
                             extensions::webstore_install::Result* reason,
                             std::string* error);

  // Initiates the enable flow for an app before it can be launched.
  void EnableInstalledApp(const extensions::Extension* extension);

  // After the ephemeral installation or enable flow are complete, attempts to
  // launch the app and notify the client of the outcome.
  void MaybeLaunchApp();

  // Launches an app. At this point, it is assumed that the app is enabled and
  // can be launched.
  void LaunchApp(const extensions::Extension* extension) const;

  // Navigates to the launch URL of a hosted app in a new browser tab.
  bool LaunchHostedApp(const extensions::Extension* extension) const;

  // Notifies the client of the launch outcome.
  void InvokeCallback(extensions::webstore_install::Result result,
                      const std::string& error);

  // Aborts the ephemeral install and notifies the client of the outcome.
  void AbortLaunch(extensions::webstore_install::Result result,
                   const std::string& error);

  // Determines whether the app can be installed ephemerally.
  void CheckEphemeralInstallPermitted();

  // Install checker callback.
  void OnInstallChecked(int check_failures);

  // WebstoreStandaloneInstaller implementation.
  virtual void InitInstallData(
      extensions::ActiveInstallData* install_data) const OVERRIDE;
  virtual bool CheckRequestorAlive() const OVERRIDE;
  virtual const GURL& GetRequestorURL() const OVERRIDE;
  virtual bool ShouldShowPostInstallUI() const OVERRIDE;
  virtual bool ShouldShowAppInstalledBubble() const OVERRIDE;
  virtual content::WebContents* GetWebContents() const OVERRIDE;
  virtual scoped_refptr<ExtensionInstallPrompt::Prompt> CreateInstallPrompt()
      const OVERRIDE;
  virtual bool CheckInlineInstallPermitted(
      const base::DictionaryValue& webstore_data,
      std::string* error) const OVERRIDE;
  virtual bool CheckRequestorPermitted(
      const base::DictionaryValue& webstore_data,
      std::string* error) const OVERRIDE;
  virtual void OnManifestParsed() OVERRIDE;
  virtual void CompleteInstall(extensions::webstore_install::Result result,
                               const std::string& error) OVERRIDE;

  // content::WebContentsObserver implementation.
  virtual void WebContentsDestroyed() OVERRIDE;

  // ExtensionEnableFlowDelegate implementation.
  virtual void ExtensionEnableFlowFinished() OVERRIDE;
  virtual void ExtensionEnableFlowAborted(bool user_initiated) OVERRIDE;

  LaunchCallback launch_callback_;

  gfx::NativeWindow parent_window_;
  scoped_ptr<content::WebContents> dummy_web_contents_;

  scoped_ptr<ExtensionEnableFlow> extension_enable_flow_;

  scoped_ptr<extensions::ExtensionInstallChecker> install_checker_;

  DISALLOW_COPY_AND_ASSIGN(EphemeralAppLauncher);
};

#endif  // CHROME_BROWSER_APPS_EPHEMERAL_APP_LAUNCHER_H_
