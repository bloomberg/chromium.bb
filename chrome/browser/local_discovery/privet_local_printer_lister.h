// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_LOCAL_PRINTER_LISTER_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_LOCAL_PRINTER_LISTER_H_

#include <map>
#include <string>

#include "base/memory/linked_ptr.h"
#include "chrome/browser/local_discovery/privet_device_lister.h"
#include "chrome/browser/local_discovery/privet_http.h"
#include "chrome/browser/local_discovery/privet_http_asynchronous_factory.h"
#include "chrome/common/local_discovery/service_discovery_client.h"
#include "net/url_request/url_request_context.h"

namespace local_discovery {

// This is an adapter to PrivetDeviceLister that finds printers and checks if
// they support Privet local printing.
class PrivetLocalPrinterLister : PrivetDeviceLister::Delegate,
                                 PrivetInfoOperation::Delegate {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void LocalPrinterChanged(bool added,
                                     const std::string& name,
                                     const DeviceDescription& description) = 0;
    virtual void LocalPrinterRemoved(const std::string& name) = 0;
    virtual void LocalPrinterCacheFlushed() = 0;
  };

  PrivetLocalPrinterLister(ServiceDiscoveryClient* service_discovery_client,
                           net::URLRequestContextGetter* request_context,
                           Delegate* delegate);
  virtual ~PrivetLocalPrinterLister();

  void Start();

  // PrivetDeviceLister::Delegate implementation.
  virtual void DeviceChanged(bool added,
                             const std::string& name,
                             const DeviceDescription& description) OVERRIDE;
  virtual void DeviceRemoved(const std::string& name) OVERRIDE;
  virtual void DeviceCacheFlushed() OVERRIDE;

  // PrivetInfoOperation::Delegate implementation.
  virtual void OnPrivetInfoDone(
      PrivetInfoOperation* operation,
      int http_code,
      const base::DictionaryValue* json_value) OVERRIDE;

 private:
  struct DeviceContext;

  void OnPrivetResolved(scoped_ptr<PrivetHTTPClient> http_client);

  typedef std::map<std::string, linked_ptr<DeviceContext> > DeviceContextMap;

  scoped_ptr<PrivetHTTPAsynchronousFactory> privet_http_factory_;
  DeviceContextMap device_contexts_;
  Delegate* delegate_;

  scoped_ptr<PrivetDeviceLister> privet_lister_;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_LOCAL_PRINTER_LISTER_H_
