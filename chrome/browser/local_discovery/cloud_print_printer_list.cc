// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/strings/stringprintf.h"
#include "chrome/browser/local_discovery/cloud_print_printer_list.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"

namespace local_discovery {

namespace {
const char kPrinterListURLFormat[] = "%s/search";
}

CloudPrintPrinterList::CloudPrintPrinterList(
    net::URLRequestContextGetter* request_context,
    const std::string& cloud_print_url,
    OAuth2TokenService* token_service,
    const std::string& account_id,
    Delegate* delegate)
    : request_context_(request_context),
      url_(base::StringPrintf(kPrinterListURLFormat, cloud_print_url.c_str())),
      delegate_(delegate),
      api_flow_(request_context_,
                token_service,
                account_id,
                url_,
                this) {
}


CloudPrintPrinterList::~CloudPrintPrinterList() {
}

void CloudPrintPrinterList::Start() {
  api_flow_.Start();
}

const CloudPrintPrinterList::PrinterDetails*
CloudPrintPrinterList::GetDetailsFor(const std::string& id) {
  PrinterIDMap::iterator found = printer_id_map_.find(id);
  if (found != printer_id_map_.end()) {
    return &printer_list_[found->second];
  }

  return NULL;
}

void CloudPrintPrinterList::OnCloudPrintAPIFlowError(
      CloudPrintBaseApiFlow* flow,
      CloudPrintBaseApiFlow::Status status) {
  delegate_->OnCloudPrintPrinterListUnavailable();
}


void CloudPrintPrinterList::OnCloudPrintAPIFlowComplete(
      CloudPrintBaseApiFlow* flow,
      const base::DictionaryValue* value) {
  const base::ListValue* printers;

  if (!value->GetList(cloud_print::kPrinterListValue, &printers)) {
    delegate_->OnCloudPrintPrinterListUnavailable();
    return;
  }

  for (base::ListValue::const_iterator i = printers->begin();
       i != printers->end();
       i++) {
    base::DictionaryValue* printer;
    PrinterDetails printer_details;

    if (!(*i)->GetAsDictionary(&printer))
      continue;

    if (!FillPrinterDetails(printer, &printer_details)) continue;

    std::pair<PrinterIDMap::iterator, bool> inserted =
        printer_id_map_.insert(
            make_pair(printer_details.id, printer_list_.size()) );

    if (inserted.second) {  // ID is new.
      printer_list_.push_back(printer_details);
    }
  }

  delegate_->OnCloudPrintPrinterListReady();
}

bool CloudPrintPrinterList::FillPrinterDetails(
    const base::DictionaryValue* printer_value,
    PrinterDetails* printer_details) {
  if (!printer_value->GetString(cloud_print::kIdValue, &printer_details->id))
    return false;

  if (!printer_value->GetString(cloud_print::kDisplayNameValue,
                                &printer_details->display_name)) {
    return false;
  }

  // Non-essential.
  printer_value->GetString(cloud_print::kPrinterDescValue,
                           &printer_details->description);

  return true;
}

CloudPrintPrinterList::PrinterDetails::PrinterDetails() {
}

CloudPrintPrinterList::PrinterDetails::~PrinterDetails() {
}

}  // namespace local_discovery
