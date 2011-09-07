// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "content/common/url_fetcher.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"

class TabContents;
class SafeWebstoreResponseParser;

// Manages inline installs requested by a page (downloads and parses metadata
// from the webstore, shows the install UI, starts the download once the user
// confirms).  Clients must implement the WebstoreInlineInstaller::Delegate
// interface to be notified when the inline install completes (successfully or
// not).
class WebstoreInlineInstaller
    : public base::RefCountedThreadSafe<WebstoreInlineInstaller>,
      public ExtensionInstallUI::Delegate,
      public URLFetcher::Delegate,
      public WebstoreInstallHelper::Delegate {
 public:
  class Delegate {
   public:
    virtual void OnInlineInstallSuccess(int install_id) = 0;
    virtual void OnInlineInstallFailure(int install_id,
                                        const std::string& error) = 0;
  };

  WebstoreInlineInstaller(TabContents* tab_contents,
                          int install_id,
                          std::string webstore_item_id,
                          GURL requestor_url,
                          Delegate* d);
  void BeginInstall();

 private:
  friend class base::RefCountedThreadSafe<WebstoreInlineInstaller>;
  friend class SafeWebstoreResponseParser;

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

  // UrlFetcher::Delegate interface implementation.
  virtual void OnURLFetchComplete(const URLFetcher* source) OVERRIDE;

  // Client callbacks for SafeWebstoreResponseParser when parsing is complete.
  void OnWebstoreResponseParseSuccess(DictionaryValue* webstore_data);
  void OnWebstoreResponseParseFailure(const std::string& error);

  // WebstoreInstallHelper::Delegate interface implementation.
  virtual void OnWebstoreParseSuccess(
      const SkBitmap& icon,
      base::DictionaryValue* parsed_manifest) OVERRIDE;
  virtual void OnWebstoreParseFailure(
      InstallHelperResultCode result_code,
      const std::string& error_message) OVERRIDE;

  // ExtensionInstallUI::Delegate interface implementation.
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

  void CompleteInstall(const std::string& error);

  TabContents* tab_contents_;
  int install_id_;
  std::string id_;
  GURL requestor_url_;
  Delegate* delegate_;

  // For fetching webstore JSON data.
  scoped_ptr<URLFetcher> webstore_data_url_fetcher_;

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
