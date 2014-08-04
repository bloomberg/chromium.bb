// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_APP_INSTALLER_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_APP_INSTALLER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/webstore_standalone_installer.h"

namespace content {
class WebContents;
}  // namespace content

namespace file_manager {

// This class is used for installing apps and extensions suggested from the
// Chrome Web Store for unsupported file types, inside Files.app.
class AppInstaller : public extensions::WebstoreStandaloneInstaller {
 public:
  AppInstaller(content::WebContents* web_contents,
               const std::string& item_id,
               Profile* profile,
               bool silent_installation,
               const Callback& callback);

 protected:
  friend class base::RefCountedThreadSafe<AppInstaller>;

  virtual ~AppInstaller();

  void OnWebContentsDestroyed(content::WebContents* web_contents);

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

 private:
  class WebContentsObserver;

  bool silent_installation_;
  Callback callback_;
  content::WebContents* web_contents_;
  scoped_ptr<WebContentsObserver> web_contents_observer_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AppInstaller);
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_APP_INSTALLER_H_
