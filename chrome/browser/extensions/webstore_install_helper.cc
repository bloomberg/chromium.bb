// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_install_helper.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "components/safe_json/safe_json_parser.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;

namespace {

const char kImageDecodeError[] = "Image decode failed";

}  // namespace

namespace extensions {

WebstoreInstallHelper::WebstoreInstallHelper(
    Delegate* delegate,
    const std::string& id,
    const std::string& manifest,
    const GURL& icon_url,
    net::URLRequestContextGetter* context_getter)
    : delegate_(delegate),
      id_(id),
      manifest_(manifest),
      icon_url_(icon_url),
      context_getter_(context_getter),
      icon_decode_complete_(false),
      manifest_parse_complete_(false),
      parse_error_(Delegate::UNKNOWN_ERROR) {
}

WebstoreInstallHelper::~WebstoreInstallHelper() {}

void WebstoreInstallHelper::Start() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  safe_json::SafeJsonParser::Parse(
      manifest_, base::Bind(&WebstoreInstallHelper::OnJSONParseSucceeded, this),
      base::Bind(&WebstoreInstallHelper::OnJSONParseFailed, this));

  if (icon_url_.is_empty()) {
    icon_decode_complete_ = true;
  } else {
    // No existing |icon_fetcher_| to avoid unbalanced AddRef().
    CHECK(!icon_fetcher_.get());
    AddRef();  // Balanced in OnFetchComplete().

    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation("webstore_install_helper", R"(
          semantics {
            sender: "Webstore Install Helper"
            description:
              "Fetches the bitmap corresponding to an extension icon."
            trigger:
              "This can happen in a few different circumstances: "
              "1-User initiated an install from the Chrome Web Store."
              "2-User initiated an inline installation from another website."
              "3-Loading of kiosk app data on Chrome OS (provided that the "
              "kiosk app is a Web Store app)."
            data:
              "The url of the icon for the extension, which includes the "
              "extension id."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
            setting:
              "There's no direct Chromium's setting to disable this, but you "
              "could uninstall all extensions and not install (or begin the "
              "installation flow for) any more."
            policy_exception_justification:
              "Not implemented, considered not useful."
          })");

    icon_fetcher_.reset(
        new chrome::BitmapFetcher(icon_url_, this, traffic_annotation));
    icon_fetcher_->Init(
        context_getter_, std::string(),
        net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
        net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_DO_NOT_SEND_COOKIES);
    icon_fetcher_->Start();
  }
}

void WebstoreInstallHelper::OnFetchComplete(const GURL& url,
                                            const SkBitmap* image) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // OnFetchComplete should only be called as icon_fetcher_ delegate to avoid
  // unbalanced Release().
  CHECK(icon_fetcher_.get());

  if (image)
    icon_ = *image;
  icon_decode_complete_ = true;
  if (icon_.empty()) {
    error_ = kImageDecodeError;
    parse_error_ = Delegate::ICON_ERROR;
  }
  icon_fetcher_.reset();

  ReportResultsIfComplete();
  Release();  // Balanced in Start().
}

void WebstoreInstallHelper::OnJSONParseSucceeded(
    std::unique_ptr<base::Value> result) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  manifest_parse_complete_ = true;
  const base::DictionaryValue* value;
  if (result->GetAsDictionary(&value))
    parsed_manifest_.reset(value->DeepCopy());
  else
    parse_error_ = Delegate::MANIFEST_ERROR;

  ReportResultsIfComplete();
}

void WebstoreInstallHelper::OnJSONParseFailed(
    const std::string& error_message) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  manifest_parse_complete_ = true;
  error_ = error_message;
  parse_error_ = Delegate::MANIFEST_ERROR;
  ReportResultsIfComplete();
}

void WebstoreInstallHelper::ReportResultsIfComplete() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!icon_decode_complete_ || !manifest_parse_complete_)
    return;

  if (error_.empty() && parsed_manifest_)
    delegate_->OnWebstoreParseSuccess(id_, icon_, parsed_manifest_.release());
  else
    delegate_->OnWebstoreParseFailure(id_, parse_error_, error_);
}

}  // namespace extensions
