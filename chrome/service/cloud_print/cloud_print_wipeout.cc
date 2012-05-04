// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/cloud_print_wipeout.h"

#include "chrome/service/cloud_print/cloud_print_consts.h"
#include "chrome/service/cloud_print/cloud_print_helpers.h"

const int kMaxWipeoutAttempts = 3;

CloudPrintWipeout::CloudPrintWipeout(Client* client,
                                     const GURL& cloud_print_server_url)
  : client_(client), cloud_print_server_url_(cloud_print_server_url) {
}
CloudPrintWipeout::~CloudPrintWipeout() {
}

void CloudPrintWipeout::UnregisterPrinters(
    const std::string& auth_token,
    const std::list<std::string>& printer_ids) {
  auth_token_ = auth_token;
  printer_ids_ = printer_ids;
  UnregisterNextPrinter();
}

void CloudPrintWipeout::UnregisterNextPrinter() {
  if (printer_ids_.empty()) {
    client_->OnUnregisterPrintersComplete();
    return;
  }

  std::string printer_id = printer_ids_.front();
  printer_ids_.pop_front();

  GURL url = CloudPrintHelpers::GetUrlForPrinterDelete(cloud_print_server_url_,
                                                       printer_id,
                                                       "connector_disabled");
  request_ = new CloudPrintURLFetcher;
  request_->StartGetRequest(url, this, kMaxWipeoutAttempts, std::string());
}

CloudPrintURLFetcher::ResponseAction CloudPrintWipeout::HandleJSONData(
    const content::URLFetcher* source,
    const GURL& url,
    base::DictionaryValue* json_data,
    bool succeeded) {
  // We don't care if delete was sucessful or not here.
  UnregisterNextPrinter();
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

void CloudPrintWipeout::OnRequestGiveUp() {
  UnregisterNextPrinter();
}

CloudPrintURLFetcher::ResponseAction CloudPrintWipeout::OnRequestAuthError() {
  // We can't recover from auth rrror. Report complition to stop service.
  client_->OnUnregisterPrintersComplete();
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

std::string CloudPrintWipeout::GetAuthHeader() {
  return CloudPrintHelpers::GetCloudPrintAuthHeader(auth_token_);
}

