// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_CLOUD_PRINT_PRINTER_LIST_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_CLOUD_PRINT_PRINTER_LIST_H_

#include <string>
#include <vector>

#include "chrome/browser/local_discovery/cloud_device_list_delegate.h"
#include "chrome/browser/local_discovery/gcd_base_api_flow.h"

namespace local_discovery {

class CloudPrintPrinterList : public CloudPrintApiFlowDelegate {
 public:
  typedef std::vector<CloudDeviceListDelegate::Device> PrinterList;
  typedef PrinterList::const_iterator iterator;

  CloudPrintPrinterList(net::URLRequestContextGetter* request_context,
                        OAuth2TokenService* token_service,
                        const std::string& account_id,
                        CloudDeviceListDelegate* delegate);
  virtual ~CloudPrintPrinterList();

  void Start();

  virtual void OnGCDAPIFlowError(GCDBaseApiFlow* flow,
                                 GCDBaseApiFlow::Status status) OVERRIDE;

  virtual void OnGCDAPIFlowComplete(
      GCDBaseApiFlow* flow,
      const base::DictionaryValue* value) OVERRIDE;

  virtual GURL GetURL() OVERRIDE;

  GCDBaseApiFlow* GetOAuth2ApiFlowForTests() { return &api_flow_; }

  const PrinterList& printer_list() const {
    return printer_list_;
  }

 private:
  bool FillPrinterDetails(const base::DictionaryValue* printer_value,
                          CloudDeviceListDelegate::Device* printer_details);

  PrinterList printer_list_;
  CloudDeviceListDelegate* delegate_;
  GCDBaseApiFlow api_flow_;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_CLOUD_PRINT_PRINTER_LIST_H_
