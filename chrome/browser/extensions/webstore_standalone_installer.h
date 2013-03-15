// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WEBSTORE_STANDALONE_INSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_WEBSTORE_STANDALONE_INSTALLER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/webstore_data_fetcher_delegate.h"
#include "chrome/browser/extensions/webstore_install_helper.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "third_party/skia/include/core/SkBitmap.h"

class GURL;

namespace base {
class DictionaryValue;
}

namespace net {
class URLFetcher;
}

namespace extensions {
class Extension;
class WebstoreDataFetcher;

// A a purely abstract base for concrete classes implementing various types of
// standalone installs:
// 1) Downloads and parses metadata from the webstore.
// 2) Optionally shows an install dialog.
// 3) Starts download once the user confirms (if confirmation was requested).
// 4) Optionally shows a post-install UI.
// Follows the Template Method pattern. Implementing subclasses must override
// the primitive hooks in the corresponding section below.

class WebstoreStandaloneInstaller
    : public base::RefCountedThreadSafe<WebstoreStandaloneInstaller>,
      public ExtensionInstallPrompt::Delegate,
      public WebstoreDataFetcherDelegate,
      public WebstoreInstaller::Delegate,
      public WebstoreInstallHelper::Delegate {
 public:
  // A callback for when the install process completes, successfully or not. If
  // there was a failure, |success| will be false and |error| may contain a
  // developer-readable error message about why it failed.
  typedef base::Callback<void(bool success, const std::string& error)> Callback;

  WebstoreStandaloneInstaller(const std::string& webstore_item_id,
                              Profile* profile,
                              const Callback& callback);
  void BeginInstall();

 protected:
  virtual ~WebstoreStandaloneInstaller();

  void AbortInstall();
  void CompleteInstall(const std::string& error);

  // Template Method's hooks to be implemented by subclasses.

  // Called at certain check points of the workflow to decide whether it makes
  // sense to proceed with installation. A requestor can be a website that
  // initiated an inline installation, or a command line option.
  virtual bool CheckRequestorAlive() const = 0;

  // Requestor's URL, if any. Should be an empty GURL if URL is meaningless
  // (e.g. for a command line option).
  virtual const GURL& GetRequestorURL() const = 0;

  // Should a new tab be opened after installation to show the newly installed
  // extension's icon?
  virtual bool ShouldShowPostInstallUI() const = 0;

  // Should pop up an "App installed" bubble after installation?
  virtual bool ShouldShowAppInstalledBubble() const = 0;

  // In the very least this should return a dummy WebContents (required
  // by some calls even when no prompt or other UI is shown). A non-dummy
  // WebContents is required if the prompt returned by CreateInstallPromt()
  // contains a navigable link(s). Returned WebContents should correspond
  // to |profile| passed into the constructor.
  virtual content::WebContents* GetWebContents() const = 0;

  // Should return an installation prompt with desired properties or NULL if
  // no prompt should be shown.
  virtual scoped_ptr<ExtensionInstallPrompt::Prompt>
      CreateInstallPrompt() const = 0;

  // Perform all necessary checks to make sure inline install is permitted,
  // e.g. in the extension's properties in the store. The implementation may
  // choose to ignore such properties.
  virtual bool CheckInlineInstallPermitted(
      const base::DictionaryValue& webstore_data,
      std::string* error) const = 0;

  // Perform all necessary checks to make sure that requestor is allowed to
  // initiate this install (e.g. that the requestor's URL matches the verified
  // author's site specified in the extension's properties in the store).
  virtual bool CheckRequestorPermitted(
      const base::DictionaryValue& webstore_data,
      std::string* error) const = 0;

  // Accessors to be used by subclasses.
  const std::string& localized_user_count() const {
    return localized_user_count_;
  }
  double average_rating() const { return average_rating_; }
  int rating_count() const { return rating_count_; }

 private:
  friend class base::RefCountedThreadSafe<WebstoreStandaloneInstaller>;
  FRIEND_TEST_ALL_PREFIXES(WebstoreStandaloneInstallerTest, DomainVerification);

  // Several delegate/client interface implementations follow. The normal flow
  // (for successful installs) is:
  //
  // 1. BeginInstall: starts the fetch of data from the webstore
  // 2. OnURLFetchComplete: starts the parsing of data from the webstore
  // 3. OnWebstoreResponseParseSuccess: starts the parsing of the manifest and
  //    fetching of icon data.
  // 4. OnWebstoreParseSuccess: shows the install UI
  // 5. InstallUIProceed: initiates the .crx download/install
  //
  // All flows (whether successful or not) end up in CompleteInstall, which
  // informs our delegate of success/failure.

  // WebstoreDataFetcherDelegate interface implementation.
  virtual void OnWebstoreRequestFailure() OVERRIDE;

  virtual void OnWebstoreResponseParseSuccess(
      base::DictionaryValue* webstore_data) OVERRIDE;

  virtual void OnWebstoreResponseParseFailure(
      const std::string& error) OVERRIDE;

  // WebstoreInstallHelper::Delegate interface implementation.
  virtual void OnWebstoreParseSuccess(
      const std::string& id,
      const SkBitmap& icon,
      base::DictionaryValue* parsed_manifest) OVERRIDE;
  virtual void OnWebstoreParseFailure(
      const std::string& id,
      InstallHelperResultCode result_code,
      const std::string& error_message) OVERRIDE;

  // ExtensionInstallPrompt::Delegate interface implementation.
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

  // WebstoreInstaller::Delegate interface implementation.
  virtual void OnExtensionInstallSuccess(const std::string& id) OVERRIDE;
  virtual void OnExtensionInstallFailure(
      const std::string& id,
      const std::string& error,
      WebstoreInstaller::FailureReason reason) OVERRIDE;

  void CreateInstallUI();

  // Input configuration.
  std::string id_;
  Callback callback_;
  Profile* profile_;

  // Installation dialog and its underlying prompt.
  scoped_ptr<ExtensionInstallPrompt> install_ui_;
  scoped_ptr<ExtensionInstallPrompt::Prompt> install_prompt_;

  // For fetching webstore JSON data.
  scoped_ptr<WebstoreDataFetcher> webstore_data_fetcher_;

  // Extracted from the webstore JSON data response.
  std::string localized_name_;
  std::string localized_description_;
  std::string localized_user_count_;
  double average_rating_;
  int rating_count_;
  scoped_ptr<DictionaryValue> webstore_data_;
  scoped_ptr<DictionaryValue> manifest_;
  SkBitmap icon_;

  // Created by CreateInstallUI() when a prompt is shown (if
  // the implementor returns a non-NULL in CreateInstallPrompt()).
  scoped_refptr<Extension> localized_extension_for_display_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebstoreStandaloneInstaller);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_WEBSTORE_STANDALONE_INSTALLER_H_
