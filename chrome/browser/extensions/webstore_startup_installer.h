// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WEBSTORE_STARTUP_INSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_WEBSTORE_STARTUP_INSTALLER_H_

#include "googleurl/src/gurl.h"
#include "webstore_standalone_installer.h"

namespace content {
class WebContents;
}

namespace extensions {

// Manages inline installs requested to be performed at startup, e.g. via a
// command line option: downloads and parses metadata from the webstore,
// optionally shows an install UI, starts the download once the user
// confirms.
//
// Clients will be notified of success or failure via the |callback| argument
// passed into the constructor.
class WebstoreStartupInstaller
    : public WebstoreStandaloneInstaller {
 public:
  typedef WebstoreStandaloneInstaller::Callback Callback;

  WebstoreStartupInstaller(const std::string& webstore_item_id,
                         Profile* profile,
                         bool show_prompt,
                         const Callback& callback);

 protected:
  friend class base::RefCountedThreadSafe<WebstoreStartupInstaller>;
  FRIEND_TEST_ALL_PREFIXES(WebstoreStartupInstallerTest, DomainVerification);

  virtual ~WebstoreStartupInstaller();

  // Implementations WebstoreStandaloneInstaller Template Method's hooks.
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
  bool show_prompt_;
  GURL dummy_requestor_url_;
  scoped_ptr<content::WebContents> dummy_web_contents_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebstoreStartupInstaller);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_WEBSTORE_STARTUP_INSTALLER_H_
