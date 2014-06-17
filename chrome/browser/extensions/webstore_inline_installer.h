// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WEBSTORE_INLINE_INSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_WEBSTORE_INLINE_INSTALLER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "content/public/browser/web_contents_observer.h"
#include "webstore_standalone_installer.h"

namespace content {
class WebContents;
}

namespace extensions {

// Manages inline installs requested by a page: downloads and parses metadata
// from the webstore, shows the install UI, starts the download once the user
// confirms, optionally transfers the user to the store if the "View details"
// link is clicked in the UI, shows the "App installed" bubble and the
// post-install UI after successful installation.
//
// Clients will be notified of success or failure via the |callback| argument
// passed into the constructor.
class WebstoreInlineInstaller
    : public WebstoreStandaloneInstaller,
      public content::WebContentsObserver {
 public:
  typedef WebstoreStandaloneInstaller::Callback Callback;

  WebstoreInlineInstaller(content::WebContents* web_contents,
                          const std::string& webstore_item_id,
                          const GURL& requestor_url,
                          const Callback& callback);

 protected:
  friend class base::RefCountedThreadSafe<WebstoreInlineInstaller>;

  virtual ~WebstoreInlineInstaller();

  // Implementations WebstoreStandaloneInstaller Template Method's hooks.
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
  // content::WebContentsObserver interface implementation.
  virtual void WebContentsDestroyed() OVERRIDE;

  // Checks whether the install is initiated by a page in a verified site
  // (which is at least a domain, but can also have a port or a path).
  static bool IsRequestorURLInVerifiedSite(const GURL& requestor_url,
                                           const std::string& verified_site);

  GURL requestor_url_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebstoreInlineInstaller);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_WEBSTORE_INLINE_INSTALLER_H_
