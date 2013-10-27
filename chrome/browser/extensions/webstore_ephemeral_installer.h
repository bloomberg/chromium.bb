// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WEBSTORE_EPHEMERAL_INSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_WEBSTORE_EPHEMERAL_INSTALLER_H_

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/extensions/webstore_standalone_installer.h"

class Profile;

namespace content {
class WebContents;
}

namespace extensions {

// WebstoreEphemeralInstaller handles the installation of ephemeral apps.
class WebstoreEphemeralInstaller
    : public extensions::WebstoreStandaloneInstaller {
 public:
  typedef WebstoreStandaloneInstaller::Callback Callback;

  static scoped_refptr<WebstoreEphemeralInstaller> CreateForLauncher(
      const std::string& webstore_item_id,
      Profile* profile,
      gfx::NativeWindow parent_window,
      const Callback& callback);

 private:
  friend class base::RefCountedThreadSafe<WebstoreEphemeralInstaller>;

  WebstoreEphemeralInstaller(const std::string& webstore_item_id,
                             Profile* profile,
                             gfx::NativeWindow parent_window,
                             const Callback& callback);

  virtual ~WebstoreEphemeralInstaller();

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
  virtual scoped_ptr<ExtensionInstallPrompt> CreateInstallUI() OVERRIDE;

  gfx::NativeWindow parent_window_;
  scoped_ptr<content::WebContents> dummy_web_contents_;

  DISALLOW_COPY_AND_ASSIGN(WebstoreEphemeralInstaller);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_WEBSTORE_EPHEMERAL_INSTALLER_H_
