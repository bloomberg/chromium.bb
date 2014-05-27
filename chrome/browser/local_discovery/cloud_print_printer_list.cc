// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/cloud_print_printer_list.h"

#include <utility>

#include "base/strings/stringprintf.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"
#include "components/cloud_devices/common/cloud_devices_urls.h"

namespace local_discovery {

CloudPrintPrinterList::CloudPrintPrinterList(
    net::URLRequestContextGetter* request_context,
    OAuth2TokenService* token_service,
    const std::string& account_id,
    CloudDeviceListDelegate* delegate)
    : delegate_(delegate),
      api_flow_(request_context,
                token_service,
                account_id,
                this) {
}

CloudPrintPrinterList::~CloudPrintPrinterList() {
}

void CloudPrintPrinterList::Start() {
  api_flow_.Start();
}

void CloudPrintPrinterList::OnGCDAPIFlowError(GCDBaseApiFlow* flow,
                                              GCDBaseApiFlow::Status status) {
  delegate_->OnDeviceListUnavailable();
}

void CloudPrintPrinterList::OnGCDAPIFlowComplete(
    GCDBaseApiFlow* flow,
    const base::DictionaryValue* value) {
  const base::ListValue* printers;

  if (!value->GetList(cloud_print::kPrinterListValue, &printers)) {
    delegate_->OnDeviceListUnavailable();
    return;
  }

  for (base::ListValue::const_iterator i = printers->begin();
       i != printers->end();
       i++) {
    base::DictionaryValue* printer;
    CloudDeviceListDelegate::Device printer_details;

    if (!(*i)->GetAsDictionary(&printer))
      continue;

    if (!FillPrinterDetails(printer, &printer_details))
      continue;

    printer_list_.push_back(printer_details);
  }

  delegate_->OnDeviceListReady();
}

GURL CloudPrintPrinterList::GetURL() {
  return cloud_devices::GetCloudPrintRelativeURL("search");
}

bool CloudPrintPrinterList::FillPrinterDetails(
    const base::DictionaryValue* printer_value,
    CloudDeviceListDelegate::Device* printer_details) {
  if (!printer_value->GetString(cloud_print::kIdValue, &printer_details->id))
    return false;

  if (!printer_value->GetString(cloud_print::kDisplayNameValue,
                                &printer_details->display_name)) {
    return false;
  }

  // Non-essential.
  printer_value->GetString(cloud_print::kPrinterDescValue,
                           &printer_details->description);

  printer_details->type = CloudDeviceListDelegate::kDeviceTypePrinter;

  return true;
}

}  // namespace local_discovery
