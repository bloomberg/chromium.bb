// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALLER_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/requirements_checker.h"
#include "chrome/common/extensions/extension.h"

class Profile;

namespace content {
class WebContents;
}

namespace extensions {


// Holds common methods and data of extension installers.
class ExtensionInstaller {
 public:
  typedef base::Callback<void(std::vector<std::string>)> RequirementsCallback;

  explicit ExtensionInstaller(Profile* profile);
  ~ExtensionInstaller();

  // Called on the UI thread to start the requirements check on the extension
  void CheckRequirements(const RequirementsCallback& callback);

#if defined(ENABLE_MANAGED_USERS)
  // Shows the managed user passphrase dialog if the managed user is not in
  // elevated state yet.
  void ShowPassphraseDialog(content::WebContents* web_contents,
                            const base::Closure& authorization_callback);

  // Runs on the UI thread. Callback from the managed user passphrase dialog.
  void OnAuthorizationResult(const base::Closure& authorization_callback,
                             bool success);
#endif

  // Checks the management policy if the extension can be installed.
  string16 CheckManagementPolicy();

  Profile* profile() const {
    return profile_;
  }

  void set_extension(const Extension* extension) {
    extension_ = extension;
  }

  scoped_refptr<const Extension> extension() {
    return extension_;
  }

 private:
  scoped_ptr<RequirementsChecker> requirements_checker_;

  // The Profile where the extension is being installed in.
  Profile* profile_;

  // The extension we're installing. We either pass it off to ExtensionService
  // on success, or drop the reference on failure.
  scoped_refptr<const Extension> extension_;

  base::WeakPtrFactory<ExtensionInstaller> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstaller);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALLER_H_
