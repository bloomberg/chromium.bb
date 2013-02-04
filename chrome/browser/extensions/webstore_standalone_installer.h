// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WEBSTORE_STANDALONE_INSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_WEBSTORE_STANDALONE_INSTALLER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/webstore_install_helper.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "content/public/browser/web_contents_observer.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "third_party/skia/include/core/SkBitmap.h"


namespace net {
class URLFetcher;
}

namespace extensions {
class Extension;
class SafeWebstoreResponseParser;

// Manages inline installs requested by a page (downloads and parses metadata
// from the webstore, shows the install UI, starts the download once the user
// confirms).  Clients must implement the WebstoreStandaloneInstaller::Delegate
// interface to be notified when the inline install completes (successfully or
// not). The client will not be notified if the WebContents that this install
// request is attached to goes away.
class WebstoreStandaloneInstaller
    : public base::RefCountedThreadSafe<WebstoreStandaloneInstaller>,
      public ExtensionInstallPrompt::Delegate,
      public content::WebContentsObserver,
      public net::URLFetcherDelegate,
      public WebstoreInstaller::Delegate,
      public WebstoreInstallHelper::Delegate {
 public:
  enum VerifiedSiteRequired {
    REQUIRE_VERIFIED_SITE,
    DO_NOT_REQUIRE_VERIFIED_SITE
  };

  enum PromptType {
    STANDARD_PROMPT,
    INLINE_PROMPT,
    SKIP_PROMPT
  };

  // A callback for when the install process completes successfully or not. If
  // there was a failure, |success| will be false and |error| may contain a
  // developer-readable error message about why it failed.
  typedef base::Callback<void(bool success, const std::string& error)> Callback;

  WebstoreStandaloneInstaller(content::WebContents* web_contents,
                              std::string webstore_item_id,
                              VerifiedSiteRequired require_verified_site,
                              PromptType prompt_type,
                              GURL requestor_url,
                              Callback callback);

  void set_skip_post_install_ui(bool skip) { skip_post_install_ui_ = skip; }

  void BeginInstall();

 private:
  friend class base::RefCountedThreadSafe<WebstoreStandaloneInstaller>;
  friend class SafeWebstoreResponseParser;
  FRIEND_TEST_ALL_PREFIXES(WebstoreStandaloneInstallerTest, DomainVerification);

  virtual ~WebstoreStandaloneInstaller();

  // Several delegate/client inteface implementations follow. The normal flow
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

  // net::URLFetcherDelegate interface implementation.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // Client callbacks for SafeWebstoreResponseParser when parsing is complete.
  void OnWebstoreResponseParseSuccess(DictionaryValue* webstore_data);
  void OnWebstoreResponseParseFailure(const std::string& error);

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

  // content::WebContentsObserver interface implementation.
  virtual void WebContentsDestroyed(
      content::WebContents* web_contents) OVERRIDE;

  // WebstoreInstaller::Delegate interface implementation.
  virtual void OnExtensionInstallSuccess(const std::string& id) OVERRIDE;
  virtual void OnExtensionInstallFailure(
      const std::string& id,
      const std::string& error,
      WebstoreInstaller::FailureReason reason) OVERRIDE;

  void CompleteInstall(const std::string& error);

  // Checks whether the install is initiated by a page in the verified site
  // (which is at least a domain, but can also have a port or a path).
  static bool IsRequestorURLInVerifiedSite(const GURL& requestor_url,
                                           const std::string& verified_site);

  std::string id_;
  bool require_verified_site_;
  PromptType prompt_type_;
  GURL requestor_url_;
  Callback callback_;
  scoped_ptr<ExtensionInstallPrompt> install_ui_;
  bool skip_post_install_ui_;

  // For fetching webstore JSON data.
  scoped_ptr<net::URLFetcher> webstore_data_url_fetcher_;

  // Extracted from the webstore JSON data response.
  std::string localized_name_;
  std::string localized_description_;
  std::string localized_user_count_;
  double average_rating_;
  int rating_count_;
  scoped_ptr<DictionaryValue> webstore_data_;
  scoped_ptr<DictionaryValue> manifest_;
  scoped_refptr<Extension> dummy_extension_;
  SkBitmap icon_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebstoreStandaloneInstaller);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_WEBSTORE_STANDALONE_INSTALLER_H_
