// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALL_WITH_PROMPT_H_
#define CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALL_WITH_PROMPT_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/webstore_standalone_installer.h"
#include "content/public/browser/page_navigator.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace extensions {

// Initiates the install of an extension from the webstore. Downloads and parses
// metadata from the webstore, shows an install UI and starts the download once
// the user confirms. No post-install UI is shown.
//
// Clients will be notified of success or failure via the |callback| argument
// passed into the constructor.
//
// Clients of this class must be trusted, as verification of the requestor is
// skipped. This class stubs out many WebstoreStandaloneInstaller abstract
// methods and can be used as a base class.
class WebstoreInstallWithPrompt : public WebstoreStandaloneInstaller,
                                  public content::PageNavigator {
 public:
  // Use this constructor when there is no parent window. The install dialog
  // will be centered on the screen.
  WebstoreInstallWithPrompt(const std::string& webstore_item_id,
                            Profile* profile,
                            const Callback& callback);

  // If this constructor is used, the parent of the install dialog will be
  // |parent_window|.
  WebstoreInstallWithPrompt(const std::string& webstore_item_id,
                            Profile* profile,
                            gfx::NativeWindow parent_window,
                            const Callback& callback);

 protected:
  friend class base::RefCountedThreadSafe<WebstoreInstallWithPrompt>;
  virtual ~WebstoreInstallWithPrompt();

  // extensions::WebstoreStandaloneInstaller overrides:
  virtual bool CheckRequestorAlive() const OVERRIDE;
  virtual const GURL& GetRequestorURL() const OVERRIDE;
  virtual bool ShouldShowPostInstallUI() const OVERRIDE;
  virtual bool ShouldShowAppInstalledBubble() const OVERRIDE;
  virtual content::WebContents* GetWebContents() const OVERRIDE;
  virtual scoped_refptr<ExtensionInstallPrompt::Prompt> CreateInstallPrompt()
      const OVERRIDE;
  virtual scoped_ptr<ExtensionInstallPrompt> CreateInstallUI() OVERRIDE;
  virtual bool CheckInlineInstallPermitted(
      const base::DictionaryValue& webstore_data,
      std::string* error) const OVERRIDE;
  virtual bool CheckRequestorPermitted(
      const base::DictionaryValue& webstore_data,
      std::string* error) const OVERRIDE;

  // content::PageNavigator overrides:
  virtual content::WebContents* OpenURL(
      const content::OpenURLParams& params) OVERRIDE;

 private:
  GURL dummy_requestor_url_;

  // A non-visible WebContents used to download data from the webstore.
  scoped_ptr<content::WebContents> dummy_web_contents_;

  gfx::NativeWindow parent_window_;

  DISALLOW_COPY_AND_ASSIGN(WebstoreInstallWithPrompt);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALL_WITH_PROMPT_H_
