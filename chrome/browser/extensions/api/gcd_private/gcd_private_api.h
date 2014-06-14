// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_GCD_PRIVATE_GCD_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_GCD_PRIVATE_GCD_PRIVATE_API_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/local_discovery/cloud_device_list_delegate.h"
#include "chrome/browser/local_discovery/gcd_api_flow.h"
#include "chrome/common/extensions/api/gcd_private.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"

namespace extensions {

class GcdPrivateAPI : public BrowserContextKeyedAPI {
 public:
  class GCDApiFlowFactoryForTests {
   public:
    virtual ~GCDApiFlowFactoryForTests() {}

    virtual scoped_ptr<local_discovery::GCDApiFlow> CreateGCDApiFlow() = 0;
  };

  explicit GcdPrivateAPI(content::BrowserContext* context);
  virtual ~GcdPrivateAPI();

  static void SetGCDApiFlowFactoryForTests(GCDApiFlowFactoryForTests* factory);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<GcdPrivateAPI>* GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<GcdPrivateAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "GcdPrivateAPI"; }

  content::BrowserContext* const browser_context_;
};

class GcdPrivateGetCloudDeviceListFunction
    : public ChromeAsyncExtensionFunction,
      public local_discovery::CloudDeviceListDelegate {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.getCloudDeviceList",
                             FEEDBACKPRIVATE_GETSTRINGS)

  GcdPrivateGetCloudDeviceListFunction();

 protected:
  virtual ~GcdPrivateGetCloudDeviceListFunction();

  // SyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

 private:
  // CloudDeviceListDelegate implementation
  virtual void OnDeviceListReady(const DeviceList& devices) OVERRIDE;
  virtual void OnDeviceListUnavailable() OVERRIDE;

  void CheckListingDone();

  int requests_succeeded_;
  int requests_failed_;
  DeviceList devices_;

  scoped_ptr<local_discovery::GCDApiFlow> printer_list_;
  scoped_ptr<local_discovery::GCDApiFlow> device_list_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_GCD_PRIVATE_GCD_PRIVATE_API_H_
