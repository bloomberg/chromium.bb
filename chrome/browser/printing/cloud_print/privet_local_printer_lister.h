// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_LOCAL_PRINTER_LISTER_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_LOCAL_PRINTER_LISTER_H_

#include <map>
#include <string>

#include "base/memory/linked_ptr.h"
#include "chrome/browser/local_discovery/service_discovery_client.h"
#include "chrome/browser/printing/cloud_print/privet_device_lister.h"
#include "chrome/browser/printing/cloud_print/privet_http.h"
#include "chrome/browser/printing/cloud_print/privet_http_asynchronous_factory.h"
#include "net/url_request/url_request_context.h"

namespace cloud_print {

// This is an adapter to PrivetDeviceLister that finds printers and checks if
// they support Privet local printing.
class PrivetLocalPrinterLister : PrivetDeviceLister::Delegate {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void LocalPrinterChanged(bool added,
                                     const std::string& name,
                                     bool has_local_printing,
                                     const DeviceDescription& description) = 0;
    virtual void LocalPrinterRemoved(const std::string& name) = 0;
    virtual void LocalPrinterCacheFlushed() = 0;
  };

  PrivetLocalPrinterLister(
      local_discovery::ServiceDiscoveryClient* service_discovery_client,
      net::URLRequestContextGetter* request_context,
      Delegate* delegate);
  virtual ~PrivetLocalPrinterLister();

  void Start();

  // Stops listening/listing, keeps the data.
  void Stop();

  const DeviceDescription* GetDeviceDescription(const std::string& name);

  // PrivetDeviceLister::Delegate implementation.
  void DeviceChanged(bool added,
                     const std::string& name,
                     const DeviceDescription& description) override;
  void DeviceRemoved(const std::string& name) override;
  void DeviceCacheFlushed() override;

 private:
  struct DeviceContext;

  typedef std::map<std::string, linked_ptr<DeviceContext> > DeviceContextMap;

  void OnPrivetInfoDone(DeviceContext* context,
                        const std::string& name,
                        const base::DictionaryValue* json_value);

  void OnPrivetResolved(const std::string& name,
                        scoped_ptr<PrivetHTTPClient> http_client);

  scoped_ptr<PrivetHTTPAsynchronousFactory> privet_http_factory_;
  DeviceContextMap device_contexts_;
  Delegate* delegate_;

  scoped_ptr<PrivetDeviceLister> privet_lister_;
};

}  // namespace cloud_print

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_LOCAL_PRINTER_LISTER_H_
