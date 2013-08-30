// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_MANAGER_INSTALLER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_MANAGER_INSTALLER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/webstore_standalone_installer.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}

namespace file_manager {

// Installer for Files.app.
class FileManagerInstaller
    : public extensions::WebstoreStandaloneInstaller {
 public:
  typedef extensions::WebstoreStandaloneInstaller::Callback Callback;

  FileManagerInstaller(content::WebContents* web_contents,
                       const std::string& webstore_item_id,
                       Profile* profile,
                       const Callback& callback);

 protected:
  friend class base::RefCountedThreadSafe<FileManagerInstaller>;

  virtual ~FileManagerInstaller();

  void OnWebContentsDestroyed(content::WebContents* web_contents);

  // WebstoreStandaloneInstaller implementation.
  virtual bool CheckRequestorAlive() const OVERRIDE;
  virtual const GURL& GetRequestorURL() const OVERRIDE;
  virtual bool ShouldShowPostInstallUI() const OVERRIDE;
  virtual bool ShouldShowAppInstalledBubble() const OVERRIDE;
  virtual content::WebContents* GetWebContents() const OVERRIDE;
  virtual scoped_ptr<ExtensionInstallPrompt::Prompt>
      CreateInstallPrompt() const OVERRIDE;
  virtual bool CheckInlineInstallPermitted(
      const base::DictionaryValue& webstore_data,
      std::string* error) const OVERRIDE;
  virtual bool CheckRequestorPermitted(
      const base::DictionaryValue& webstore_data,
      std::string* error) const OVERRIDE;

 private:
  class WebContentsObserver;

  Callback callback_;
  content::WebContents* web_contents_;
  scoped_ptr<WebContentsObserver> web_contents_observer_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FileManagerInstaller);
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_MANAGER_INSTALLER_H_
