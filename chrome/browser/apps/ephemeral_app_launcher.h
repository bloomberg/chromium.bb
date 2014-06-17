// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_EPHEMERAL_APP_LAUNCHER_H_
#define CHROME_BROWSER_APPS_EPHEMERAL_APP_LAUNCHER_H_

#include <string>

#include "base/basictypes.h"
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
class ExtensionRegistry;
}

// EphemeralAppLauncher manages the launching of ephemeral apps. It handles
// display of a prompt, initiates install of the app (if necessary) and finally
// launches the app.
class EphemeralAppLauncher : public extensions::WebstoreStandaloneInstaller,
                             public content::WebContentsObserver,
                             public ExtensionEnableFlowDelegate {
 public:
  typedef WebstoreStandaloneInstaller::Callback Callback;

  // Create for the app launcher.
  static scoped_refptr<EphemeralAppLauncher> CreateForLauncher(
      const std::string& webstore_item_id,
      Profile* profile,
      gfx::NativeWindow parent_window,
      const Callback& callback);

  // Create for a link within a browser tab.
  static scoped_refptr<EphemeralAppLauncher> CreateForLink(
      const std::string& webstore_item_id,
      content::WebContents* web_contents);

  // Initiate app launch.
  void Start();

 private:
  friend class base::RefCountedThreadSafe<EphemeralAppLauncher>;

  EphemeralAppLauncher(const std::string& webstore_item_id,
                       Profile* profile,
                       gfx::NativeWindow parent_window,
                       const Callback& callback);
  EphemeralAppLauncher(const std::string& webstore_item_id,
                       content::WebContents* web_contents,
                       const Callback& callback);

  virtual ~EphemeralAppLauncher();

  void LaunchApp(const extensions::Extension* extension) const;

  // WebstoreStandaloneInstaller implementation.
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
  virtual bool CheckInstallValid(
      const base::DictionaryValue& manifest,
      std::string* error) OVERRIDE;
  virtual scoped_ptr<ExtensionInstallPrompt> CreateInstallUI() OVERRIDE;
  virtual scoped_ptr<extensions::WebstoreInstaller::Approval>
      CreateApproval() const OVERRIDE;
  virtual void CompleteInstall(const std::string& error) OVERRIDE;

  // content::WebContentsObserver implementation.
  virtual void WebContentsDestroyed() OVERRIDE;

  // ExtensionEnableFlowDelegate implementation.
  virtual void ExtensionEnableFlowFinished() OVERRIDE;
  virtual void ExtensionEnableFlowAborted(bool user_initiated) OVERRIDE;

  gfx::NativeWindow parent_window_;
  scoped_ptr<content::WebContents> dummy_web_contents_;

  // Created in CheckInstallValid().
  scoped_refptr<extensions::Extension> extension_;

  scoped_ptr<ExtensionEnableFlow> extension_enable_flow_;

  DISALLOW_COPY_AND_ASSIGN(EphemeralAppLauncher);
};

#endif  // CHROME_BROWSER_APPS_EPHEMERAL_APP_LAUNCHER_H_
