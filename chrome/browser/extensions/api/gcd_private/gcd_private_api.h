// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_GCD_PRIVATE_GCD_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_GCD_PRIVATE_GCD_PRIVATE_API_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/local_discovery/cloud_device_list_delegate.h"
#include "chrome/common/extensions/api/gcd_private.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"

namespace local_discovery {
class GCDApiFlow;
}

namespace extensions {

class GcdPrivateAPIImpl;

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
  friend class GcdPrivateAPIImpl;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "GcdPrivateAPI"; }

  scoped_ptr<GcdPrivateAPIImpl> impl_;
};

class GcdPrivateGetCloudDeviceListFunction
    : public ChromeAsyncExtensionFunction,
      public local_discovery::CloudDeviceListDelegate {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.getCloudDeviceList",
                             GCDPRIVATE_GETCLOUDDEVICELIST)

  GcdPrivateGetCloudDeviceListFunction();

 protected:
  virtual ~GcdPrivateGetCloudDeviceListFunction();

  // AsyncExtensionFunction overrides.
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

class GcdPrivateQueryForNewLocalDevicesFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.queryForNewLocalDevices",
                             GCDPRIVATE_QUERYFORNEWLOCALDEVICES)

  GcdPrivateQueryForNewLocalDevicesFunction();

 protected:
  virtual ~GcdPrivateQueryForNewLocalDevicesFunction();

  // SyncExtensionFunction overrides.
  virtual bool RunSync() OVERRIDE;
};

class GcdPrivatePrefetchWifiPasswordFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.prefetchWifiPassword",
                             GCDPRIVATE_PREFETCHWIFIPASSWORD)

  GcdPrivatePrefetchWifiPasswordFunction();

 protected:
  virtual ~GcdPrivatePrefetchWifiPasswordFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

  void OnResponse(bool response);
};

class GcdPrivateGetPrefetchedWifiNameListFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.getPrefetchedWifiNameList",
                             GCDPRIVATE_GETPREFETCHEDWIFINAMELIST);

  GcdPrivateGetPrefetchedWifiNameListFunction();

 protected:
  virtual ~GcdPrivateGetPrefetchedWifiNameListFunction();

  // SyncExtensionFunction overrides.
  virtual bool RunSync() OVERRIDE;
};

class GcdPrivateEstablishSessionFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.establishSession",
                             GCDPRIVATE_ESTABLISHSESSION)

  GcdPrivateEstablishSessionFunction();

 protected:
  virtual ~GcdPrivateEstablishSessionFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

 private:
  void OnConfirmCodeCallback(
      int session_id,
      api::gcd_private::Status status,
      const std::string& confirm_code,
      api::gcd_private::ConfirmationType confirmation_type);
};

class GcdPrivateConfirmCodeFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.confirmCode", GCDPRIVATE_CONFIRMCODE)

  GcdPrivateConfirmCodeFunction();

 protected:
  virtual ~GcdPrivateConfirmCodeFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

 private:
  void OnSessionEstablishedCallback(api::gcd_private::Status status);
};

class GcdPrivateSendMessageFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.sendMessage", GCDPRIVATE_SENDMESSAGE)

  GcdPrivateSendMessageFunction();

 protected:
  virtual ~GcdPrivateSendMessageFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

 private:
  void OnMessageSentCallback(api::gcd_private::Status status,
                             const base::DictionaryValue& value);
};

class GcdPrivateTerminateSessionFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.terminateSession",
                             GCDPRIVATE_TERMINATESESSION)

  GcdPrivateTerminateSessionFunction();

 protected:
  virtual ~GcdPrivateTerminateSessionFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;
};

class GcdPrivateGetCommandDefinitionsFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.getCommandDefinitions",
                             GCDPRIVATE_GETCOMMANDDEFINITIONS)

  GcdPrivateGetCommandDefinitionsFunction();

 protected:
  virtual ~GcdPrivateGetCommandDefinitionsFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

 private:
};

class GcdPrivateInsertCommandFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.insertCommand",
                             GCDPRIVATE_INSERTCOMMAND)

  GcdPrivateInsertCommandFunction();

 protected:
  virtual ~GcdPrivateInsertCommandFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

 private:
};

class GcdPrivateGetCommandFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.getCommand", GCDPRIVATE_GETCOMMAND)

  GcdPrivateGetCommandFunction();

 protected:
  virtual ~GcdPrivateGetCommandFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

 private:
};

class GcdPrivateCancelCommandFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.cancelCommand",
                             GCDPRIVATE_CANCELCOMMAND)

  GcdPrivateCancelCommandFunction();

 protected:
  virtual ~GcdPrivateCancelCommandFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

 private:
};

class GcdPrivateGetCommandsListFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.getCommandsList",
                             GCDPRIVATE_GETCOMMANDSLIST)

  GcdPrivateGetCommandsListFunction();

 protected:
  virtual ~GcdPrivateGetCommandsListFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

 private:
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_GCD_PRIVATE_GCD_PRIVATE_API_H_
