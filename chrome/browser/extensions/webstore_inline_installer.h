// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WEBSTORE_INLINE_INSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_WEBSTORE_INLINE_INSTALLER_H_
#pragma once

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/webstore_install_helper.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"

class SafeWebstoreResponseParser;

// Manages inline installs requested by a page (downloads and parses metadata
// from the webstore, shows the install UI, starts the download once the user
// confirms).  Clients must implement the WebstoreInlineInstaller::Delegate
// interface to be notified when the inline install completes (successfully or
// not). The client will not be notified if the WebContents that this install
// request is attached to goes away.
class WebstoreInlineInstaller
    : public base::RefCountedThreadSafe<WebstoreInlineInstaller>,
      public ExtensionInstallUI::Delegate,
      public content::WebContentsObserver,
      public content::URLFetcherDelegate,
      public WebstoreInstaller::Delegate,
      public WebstoreInstallHelper::Delegate {
 public:
  class Delegate {
   public:
    virtual void OnInlineInstallSuccess(int install_id,
                                        int return_route_id) = 0;
    virtual void OnInlineInstallFailure(int install_id,
                                        int return_route_id,
                                        const std::string& error) = 0;
  };

  WebstoreInlineInstaller(content::WebContents* web_contents,
                          int install_id,
                          int return_route_id,
                          std::string webstore_item_id,
                          GURL requestor_url,
                          Delegate* d);
  void BeginInstall();

 private:
  friend class base::RefCountedThreadSafe<WebstoreInlineInstaller>;
  friend class SafeWebstoreResponseParser;
  FRIEND_TEST_ALL_PREFIXES(WebstoreInlineInstallerTest, DomainVerification);

  virtual ~WebstoreInlineInstaller();

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

  // content::URLFetcherDelegate interface implementation.
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

  // ExtensionInstallUI::Delegate interface implementation.
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

  // content::WebContentsObserver interface implementation.
  virtual void WebContentsDestroyed(
      content::WebContents* web_contents) OVERRIDE;

  // WebstoreInstaller::Delegate interface implementation.
  virtual void OnExtensionInstallSuccess(const std::string& id) OVERRIDE;
  virtual void OnExtensionInstallFailure(const std::string& id,
                                         const std::string& error) OVERRIDE;

  void CompleteInstall(const std::string& error);

  // Checks whether the install is initiated by a page in the verified site
  // (which is at least a domain, but can also have a port or a path).
  static bool IsRequestorURLInVerifiedSite(const GURL& requestor_url,
                                           const std::string& verified_site);

  int install_id_;
  int return_route_id_;
  std::string id_;
  GURL requestor_url_;
  Delegate* delegate_;
  scoped_ptr<ExtensionInstallUI> install_ui_;

  // For fetching webstore JSON data.
  scoped_ptr<content::URLFetcher> webstore_data_url_fetcher_;

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

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebstoreInlineInstaller);
};

#endif  // CHROME_BROWSER_EXTENSIONS_WEBSTORE_INLINE_INSTALLER_H_
