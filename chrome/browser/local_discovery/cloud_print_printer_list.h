// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_CLOUD_PRINT_PRINTER_LIST_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_CLOUD_PRINT_PRINTER_LIST_H_

#include <map>
#include <string>
#include <vector>

#include "chrome/browser/local_discovery/cloud_print_base_api_flow.h"

namespace local_discovery {

class CloudPrintPrinterList : public CloudPrintBaseApiFlow::Delegate {
 public:
  class Delegate {
   public:
    ~Delegate() {}

    virtual void OnCloudPrintPrinterListReady() = 0;
    virtual void OnCloudPrintPrinterListUnavailable() = 0;
  };

  struct PrinterDetails {
    PrinterDetails();
    ~PrinterDetails();

    std::string id;
    std::string display_name;
    std::string description;
    // TODO(noamsml): std::string user;
  };

  typedef std::vector<PrinterDetails> PrinterList;
  typedef PrinterList::const_iterator iterator;

  CloudPrintPrinterList(net::URLRequestContextGetter* request_context,
                        const std::string& cloud_print_url,
                        OAuth2TokenService* token_service,
                        Delegate* delegate);
  virtual ~CloudPrintPrinterList();

  void Start();

  virtual void OnCloudPrintAPIFlowError(
      CloudPrintBaseApiFlow* flow,
      CloudPrintBaseApiFlow::Status status) OVERRIDE;

  virtual void OnCloudPrintAPIFlowComplete(
      CloudPrintBaseApiFlow* flow,
      const base::DictionaryValue* value) OVERRIDE;

  const PrinterDetails* GetDetailsFor(const std::string& id);

  iterator begin() { return printer_list_.begin(); }
  iterator end() { return printer_list_.end(); }

  CloudPrintBaseApiFlow* GetOAuth2ApiFlowForTests() {
    return &api_flow_;
  }

 private:
  typedef std::map<std::string /*ID*/, int /* index in printer_list_ */>
      PrinterIDMap;

  bool FillPrinterDetails(const base::DictionaryValue* printer_value,
                          PrinterDetails* printer_details);

  scoped_refptr<net::URLRequestContextGetter> request_context_;
  GURL url_;
  PrinterIDMap printer_id_map_;
  PrinterList printer_list_;
  Delegate* delegate_;
  CloudPrintBaseApiFlow api_flow_;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_CLOUD_PRINT_PRINTER_LIST_H_
