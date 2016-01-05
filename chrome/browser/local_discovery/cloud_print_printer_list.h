// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_CLOUD_PRINT_PRINTER_LIST_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_CLOUD_PRINT_PRINTER_LIST_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "chrome/browser/local_discovery/gcd_api_flow.h"

namespace local_discovery {

class CloudPrintPrinterList : public CloudPrintApiFlowRequest {
 public:
  struct Device {
    Device();
    ~Device();

    std::string id;
    std::string display_name;
    std::string description;
  };
  typedef std::vector<Device> DeviceList;

  class Delegate {
   public:
    Delegate();
    virtual ~Delegate();

    virtual void OnDeviceListReady(const DeviceList& devices) = 0;
    virtual void OnDeviceListUnavailable() = 0;
  };

  explicit CloudPrintPrinterList(Delegate* delegate);
  ~CloudPrintPrinterList() override;

  void OnGCDAPIFlowError(GCDApiFlow::Status status) override;

  void OnGCDAPIFlowComplete(const base::DictionaryValue& value) override;

  GURL GetURL() override;

 private:
  bool FillPrinterDetails(const base::DictionaryValue& printer_value,
                          Device* printer_details);

  Delegate* delegate_;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_CLOUD_PRINT_PRINTER_LIST_H_
