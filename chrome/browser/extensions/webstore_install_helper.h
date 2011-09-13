// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALL_HELPER_H_
#define CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALL_HELPER_H_
#pragma once

#include "content/browser/utility_process_host.h"
#include "content/common/net/url_fetcher.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"

class UtilityProcessHost;
class SkBitmap;

namespace base {
class DictionaryValue;
class ListValue;
}

// This is a class to help dealing with webstore-provided data. It manages
// sending work to the utility process for parsing manifests and
// fetching/decoding icon data. Clients must implement the
// WebstoreInstallHelper::Delegate interface to receive the parsed data.
class WebstoreInstallHelper : public UtilityProcessHost::Client,
                              public URLFetcher::Delegate {
 public:
  class Delegate {
   public:
    enum InstallHelperResultCode {
      UNKNOWN_ERROR,
      ICON_ERROR,
      MANIFEST_ERROR
    };

    // Called when we've successfully parsed the manifest and decoded the icon
    // in the utility process. Ownership of parsed_manifest is transferred.
    virtual void OnWebstoreParseSuccess(
        const SkBitmap& icon,
        base::DictionaryValue* parsed_manifest) = 0;

    // Called to indicate a parse failure. The |result_code| parameter should
    // indicate whether the problem was with the manifest or icon.
    virtual void OnWebstoreParseFailure(
        InstallHelperResultCode result_code,
        const std::string& error_message) = 0;
  };

  // Only one of |icon_data| (based64-encoded icon data) or |icon_url| can be
  // specified, but it is legal for both to be empty.
  WebstoreInstallHelper(Delegate* delegate,
                        const std::string& manifest,
                        const std::string& icon_data,
                        const GURL& icon_url,
                        net::URLRequestContextGetter* context_getter);
  void Start();

 private:
  virtual ~WebstoreInstallHelper();

  void StartWorkOnIOThread();
  void StartFetchedImageDecode();
  void ReportResultsIfComplete();
  void ReportResultFromUIThread();

  // Implementing the URLFetcher::Delegate interface.
  virtual void OnURLFetchComplete(const URLFetcher* source) OVERRIDE;

  // Implementing pieces of the UtilityProcessHost::Client interface.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Message handlers.
  void OnDecodeImageSucceeded(const SkBitmap& decoded_image);
  void OnDecodeImageFailed();
  void OnJSONParseSucceeded(const base::ListValue& wrapper);
  void OnJSONParseFailed(const std::string& error_message);

  // The client who we'll report results back to.
  Delegate* delegate_;

  // The manifest to parse.
  std::string manifest_;

  // Only one of these should be non-empty. If |icon_base64_data_| is non-emtpy,
  // it's a base64-encoded string that needs to be parsed into an SkBitmap. If
  // |icon_url_| is non-empty, it needs to be fetched and decoded into an
  // SkBitmap.
  std::string icon_base64_data_;
  GURL icon_url_;
  std::vector<unsigned char> fetched_icon_data_;

  // For fetching the icon, if needed.
  scoped_ptr<URLFetcher> url_fetcher_;
  net::URLRequestContextGetter* context_getter_; // Only usable on UI thread.

  UtilityProcessHost* utility_host_;

  // Flags for whether we're done doing icon decoding and manifest parsing.
  bool icon_decode_complete_;
  bool manifest_parse_complete_;

  // The results of succesful decoding/parsing.
  SkBitmap icon_;
  scoped_ptr<base::DictionaryValue> parsed_manifest_;

  // A details string for keeping track of any errors.
  std::string error_;

  // A code to distinguish between an error with the icon, and an error with the
  // manifest.
  Delegate::InstallHelperResultCode parse_error_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALL_HELPER_H_
