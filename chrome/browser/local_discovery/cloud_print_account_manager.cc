// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/cloud_print_account_manager.h"

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"
#include "url/gurl.h"

namespace local_discovery {

namespace {
// URL relative to cloud print root
const char kCloudPrintRequestURLFormat[] = "%s/list?proxy=none";
const char kCloudPrintKeyUsers[] = "request.users";
const char kCloudPrintKeyXsrfToken[] = "xsrf_token";
}  // namespace

CloudPrintAccountManager::CloudPrintAccountManager(
    net::URLRequestContextGetter* request_context,
    const std::string& cloud_print_url,
    int token_user_index,
    const AccountsCallback& callback)
    : flow_(request_context,
            token_user_index,
            GURL(base::StringPrintf(kCloudPrintRequestURLFormat,
                                    cloud_print_url.c_str())),
            this),
      callback_(callback) {
}

CloudPrintAccountManager::~CloudPrintAccountManager() {
}

void CloudPrintAccountManager::Start() {
  flow_.Start();
}

// If an error occurs or the user is not logged in, return an empty user list to
// signify cookie-based accounts should not be used.
void CloudPrintAccountManager::ReportEmptyUserList() {
  callback_.Run(std::vector<std::string>(), "");
}

void CloudPrintAccountManager::OnCloudPrintAPIFlowError(
    CloudPrintBaseApiFlow* flow,
    CloudPrintBaseApiFlow::Status status) {
  ReportEmptyUserList();
}

void CloudPrintAccountManager::OnCloudPrintAPIFlowComplete(
    CloudPrintBaseApiFlow* flow,
    const base::DictionaryValue* value) {
  bool success = false;

  std::string xsrf_token;
  const base::ListValue* users = NULL;
  std::vector<std::string> users_vector;

  if (!value->GetBoolean(cloud_print::kSuccessValue, &success) ||
      !value->GetList(kCloudPrintKeyUsers, &users) ||
      !value->GetString(kCloudPrintKeyXsrfToken, &xsrf_token) ||
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
