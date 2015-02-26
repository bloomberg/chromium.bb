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
  ~GcdPrivateAPI() override;

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
  ~GcdPrivateGetCloudDeviceListFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
  // CloudDeviceListDelegate implementation
  void OnDeviceListReady(const DeviceList& devices) override;
  void OnDeviceListUnavailable() override;

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
  ~GcdPrivateQueryForNewLocalDevicesFunction() override;

  // SyncExtensionFunction overrides.
  bool RunSync() override;
};

class GcdPrivatePrefetchWifiPasswordFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.prefetchWifiPassword",
                             GCDPRIVATE_PREFETCHWIFIPASSWORD)

  GcdPrivatePrefetchWifiPasswordFunction();

 protected:
  ~GcdPrivatePrefetchWifiPasswordFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

  void OnResponse(bool response);
};

class GcdPrivateGetPrefetchedWifiNameListFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.getPrefetchedWifiNameList",
                             GCDPRIVATE_GETPREFETCHEDWIFINAMELIST);

  GcdPrivateGetPrefetchedWifiNameListFunction();

 protected:
  ~GcdPrivateGetPrefetchedWifiNameListFunction() override;

  // SyncExtensionFunction overrides.
  bool RunSync() override;
};

class GcdPrivateEstablishSessionFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.establishSession",
                             GCDPRIVATE_ESTABLISHSESSION)

  GcdPrivateEstablishSessionFunction();

 protected:
  ~GcdPrivateEstablishSessionFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
  void OnSessionInitialized(
      int session_id,
      api::gcd_private::Status status,
      const std::vector<api::gcd_private::PairingType>& pairing_types);
};

class GcdPrivateCreateSessionFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.createSession",
                             GCDPRIVATE_ESTABLISHSESSION)

  GcdPrivateCreateSessionFunction();

 protected:
  ~GcdPrivateCreateSessionFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
  void OnSessionInitialized(
      int session_id,
      api::gcd_private::Status status,
      const std::vector<api::gcd_private::PairingType>& pairing_types);
};

class GcdPrivateStartPairingFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.startPairing", GCDPRIVATE_STARTPAIRING)

  GcdPrivateStartPairingFunction();

 protected:
  ~GcdPrivateStartPairingFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
  void OnPairingStarted(api::gcd_private::Status status);
};

class GcdPrivateConfirmCodeFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.confirmCode", GCDPRIVATE_CONFIRMCODE)

  GcdPrivateConfirmCodeFunction();

 protected:
  ~GcdPrivateConfirmCodeFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
  void OnCodeConfirmed(api::gcd_private::Status status);
};

class GcdPrivateSendMessageFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.sendMessage", GCDPRIVATE_SENDMESSAGE)

  GcdPrivateSendMessageFunction();

 protected:
  ~GcdPrivateSendMessageFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

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
  ~GcdPrivateTerminateSessionFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;
};

class GcdPrivateGetCommandDefinitionsFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.getCommandDefinitions",
                             GCDPRIVATE_GETCOMMANDDEFINITIONS)

  GcdPrivateGetCommandDefinitionsFunction();

 protected:
  ~GcdPrivateGetCommandDefinitionsFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
};

class GcdPrivateInsertCommandFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.insertCommand",
                             GCDPRIVATE_INSERTCOMMAND)

  GcdPrivateInsertCommandFunction();

 protected:
  ~GcdPrivateInsertCommandFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
};

class GcdPrivateGetCommandFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.getCommand", GCDPRIVATE_GETCOMMAND)

  GcdPrivateGetCommandFunction();

 protected:
  ~GcdPrivateGetCommandFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
};

class GcdPrivateCancelCommandFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.cancelCommand",
                             GCDPRIVATE_CANCELCOMMAND)

  GcdPrivateCancelCommandFunction();

 protected:
  ~GcdPrivateCancelCommandFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
};

class GcdPrivateGetCommandsListFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.getCommandsList",
                             GCDPRIVATE_GETCOMMANDSLIST)

  GcdPrivateGetCommandsListFunction();

 protected:
  ~GcdPrivateGetCommandsListFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_GCD_PRIVATE_GCD_PRIVATE_API_H_
