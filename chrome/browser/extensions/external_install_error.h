// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_INSTALL_ERROR_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_INSTALL_ERROR_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/webstore_data_fetcher_delegate.h"

class Browser;
class ExtensionInstallUI;
class GlobalError;
class GlobalErrorService;

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
class ExternalInstallManager;
class WebstoreDataFetcher;

// An error to show the user an extension has been externally installed. The
// error will automatically fetch data about the extension from the webstore (if
// possible) and will handle adding itself to the GlobalErrorService when
// initialized and removing itself from the GlobalErrorService upon
// destruction.
class ExternalInstallError : public ExtensionInstallPrompt::Delegate,
                             public WebstoreDataFetcherDelegate {
 public:
  // The possible types of errors to show. A menu alert adds a menu item to the
  // wrench, which spawns an extension install dialog when clicked. The bubble
  // alert also adds an item, but spawns a bubble instead (less invasive and
  // easier to dismiss).
  enum AlertType {
    BUBBLE_ALERT,
    MENU_ALERT
  };

  ExternalInstallError(content::BrowserContext* browser_context,
                       const std::string& extension_id,
                       AlertType error_type,
                       ExternalInstallManager* manager);
  virtual ~ExternalInstallError();

  // ExtensionInstallPrompt::Delegate implementation.
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

  // Show the associated dialog. This should only be called once the dialog is
  // ready.
  void ShowDialog(Browser* browser);

  // Return the associated extension, or NULL.
  const Extension* GetExtension() const;

  const std::string& extension_id() const { return extension_id_; }
  AlertType alert_type() const { return alert_type_; }

 private:
  // WebstoreDataFetcherDelegate implementation.
  virtual void OnWebstoreRequestFailure() OVERRIDE;
  virtual void OnWebstoreResponseParseSuccess(
      scoped_ptr<base::DictionaryValue> webstore_data) OVERRIDE;
  virtual void OnWebstoreResponseParseFailure(
      const std::string& error) OVERRIDE;

  // Called when data fetching has completed (either successfully or not).
  void OnFetchComplete();

  // Called when the dialog has been successfully populated, and is ready to be
  // shown.
  void OnDialogReady(const ExtensionInstallPrompt::ShowParams& show_params,
                     ExtensionInstallPrompt::Delegate* prompt_delegate,
                     scoped_refptr<ExtensionInstallPrompt::Prompt> prompt);

  // The associated BrowserContext.
  content::BrowserContext* browser_context_;

  // The id of the external extension.
  std::string extension_id_;

  // The type of alert to show the user.
  AlertType alert_type_;

  // The owning ExternalInstallManager.
  ExternalInstallManager* manager_;

  // The associated GlobalErrorService.
  GlobalErrorService* error_service_;

  // The UI for showing the error.
  scoped_ptr<ExtensionInstallPrompt> install_ui_;
  scoped_refptr<ExtensionInstallPrompt::Prompt> prompt_;

  // The UI for the given error, which will take the form of either a menu
  // alert or a bubble alert (depending on the |alert_type_|.
  scoped_ptr<GlobalError> global_error_;

  // The WebstoreDataFetcher to use in order to populate the error with webstore
  // information of the extension.
  scoped_ptr<WebstoreDataFetcher> webstore_data_fetcher_;

  base::WeakPtrFactory<ExternalInstallError> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExternalInstallError);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_INSTALL_ERROR_H_
