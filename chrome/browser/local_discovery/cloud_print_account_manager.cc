// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/cloud_print_account_manager.h"

#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

namespace local_discovery {

namespace {
// URL relative to cloud print root
const char kCloudPrintRequestURLFormat[] = "%s/list?proxy=none&user=%d";
const char kCloudPrintKeyUsers[] = "request.users";
const char kCloudPrintKeyXsrfToken[] = "xsrf_token";
}  // namespace

CloudPrintAccountManager::CloudPrintAccountManager(
    net::URLRequestContextGetter* request_context,
    const std::string& cloud_print_url,
    int token_user_index,
    const AccountsCallback& callback)
    : request_context_(request_context), cloud_print_url_(cloud_print_url),
      token_user_index_(token_user_index), callback_(callback) {
}

CloudPrintAccountManager::~CloudPrintAccountManager() {
}

void CloudPrintAccountManager::Start() {
  GURL url(base::StringPrintf(kCloudPrintRequestURLFormat,
                              cloud_print_url_.c_str(),
                              token_user_index_));
  url_fetcher_.reset(net::URLFetcher::Create(url, net::URLFetcher::POST, this));
  url_fetcher_->SetRequestContext(request_context_.get());
  url_fetcher_->SetUploadData("", "");
  url_fetcher_->AddExtraRequestHeader(
      cloud_print::kChromeCloudPrintProxyHeader);
  url_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES);
  url_fetcher_->Start();
}

// If an error occurs or the user is not logged in, return an empty user list to
// signify cookie-based accounts should not be used.
void CloudPrintAccountManager::ReportEmptyUserList() {
  callback_.Run(std::vector<std::string>(), "");
}

void CloudPrintAccountManager::OnURLFetchComplete(
    const net::URLFetcher* source) {
  std::string response_str;

  if (source->GetStatus().status() != net::URLRequestStatus::SUCCESS ||
      source->GetResponseCode() != net::HTTP_OK ||
      !source->GetResponseAsString(&response_str)) {
    ReportEmptyUserList();
    return;
  }

  base::JSONReader reader;
  scoped_ptr<const base::Value> value(reader.Read(response_str));
  const base::DictionaryValue* dictionary_value;
  bool success = false;

  std::string xsrf_token;
  const base::ListValue* users = NULL;
  std::vector<std::string> users_vector;

  if (!value.get() ||
      !value->GetAsDictionary(&dictionary_value) ||
      !dictionary_value->GetBoolean(cloud_print::kSuccessValue, &success) ||
      !dictionary_value->GetList(kCloudPrintKeyUsers, &users) ||
      !dictionary_value->GetString(kCloudPrintKeyXsrfToken, &xsrf_token) ||
      !success) {
    ReportEmptyUserList();
    return;
  }

  for (size_t i = 0; i < users->GetSize(); i++) {
    std::string user;
    if (!users->GetString(i, &user)) {
      // If we can't read a user from the list, send the users we do recognize
      // and the XSRF token from the server.
      break;
    }

    users_vector.push_back(user);
  }

  callback_.Run(users_vector, xsrf_token);
}

}  // namespace local_discovery
