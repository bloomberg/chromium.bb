// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_API_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "device/bluetooth/bluetooth_device.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"

// Implementations for chrome.easyUnlockPrivate API functions.

namespace base {
class Timer;
}

namespace content {
class BrowserContext;
}

namespace cryptauth {
class Connection;
class ExternalDeviceInfo;
class SecureMessageDelegate;
}

namespace proximity_auth {
class BluetoothLowEnergyConnectionFinder;
}

namespace extensions {

class EasyUnlockPrivateConnectionManager;
class EasyUnlockPrivateCryptoDelegate;

class EasyUnlockPrivateAPI : public BrowserContextKeyedAPI {
 public:
  static BrowserContextKeyedAPIFactory<EasyUnlockPrivateAPI>*
      GetFactoryInstance();

  static const bool kServiceRedirectedInIncognito = true;

  explicit EasyUnlockPrivateAPI(content::BrowserContext* context);
  ~EasyUnlockPrivateAPI() override;

  EasyUnlockPrivateCryptoDelegate* GetCryptoDelegate();

  EasyUnlockPrivateConnectionManager* get_connection_manager() {
    return connection_manager_.get();
  }

 private:
  friend class BrowserContextKeyedAPIFactory<EasyUnlockPrivateAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "EasyUnlockPrivate"; }

  // KeyedService implementation.
  void Shutdown() override;

  std::unique_ptr<EasyUnlockPrivateCryptoDelegate> crypto_delegate_;

  std::unique_ptr<EasyUnlockPrivateConnectionManager> connection_manager_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateAPI);
};

class EasyUnlockPrivateGetStringsFunction : public UIThreadExtensionFunction {
 public:
  EasyUnlockPrivateGetStringsFunction();

 protected:
  ~EasyUnlockPrivateGetStringsFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.getStrings",
                             EASYUNLOCKPRIVATE_GETSTRINGS)

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateGetStringsFunction);
};

class EasyUnlockPrivatePerformECDHKeyAgreementFunction
    : public UIThreadExtensionFunction {
 public:
  EasyUnlockPrivatePerformECDHKeyAgreementFunction();

 protected:
  ~EasyUnlockPrivatePerformECDHKeyAgreementFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnData(const std::string& secret_key);

  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.performECDHKeyAgreement",
                             EASYUNLOCKPRIVATE_PERFORMECDHKEYAGREEMENT)

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivatePerformECDHKeyAgreementFunction);
};

class EasyUnlockPrivateGenerateEcP256KeyPairFunction
    : public UIThreadExtensionFunction {
 public:
  EasyUnlockPrivateGenerateEcP256KeyPairFunction();

 protected:
  ~EasyUnlockPrivateGenerateEcP256KeyPairFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnData(const std::string& public_key,
              const std::string& private_key);

  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.generateEcP256KeyPair",
                             EASYUNLOCKPRIVATE_GENERATEECP256KEYPAIR)

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateGenerateEcP256KeyPairFunction);
};

class EasyUnlockPrivateCreateSecureMessageFunction
    : public UIThreadExtensionFunction {
 public:
  EasyUnlockPrivateCreateSecureMessageFunction();

 protected:
  ~EasyUnlockPrivateCreateSecureMessageFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnData(const std::string& message);

  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.createSecureMessage",
                             EASYUNLOCKPRIVATE_CREATESECUREMESSAGE)

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateCreateSecureMessageFunction);
};

class EasyUnlockPrivateUnwrapSecureMessageFunction
    : public UIThreadExtensionFunction {
 public:
  EasyUnlockPrivateUnwrapSecureMessageFunction();

 protected:
  ~EasyUnlockPrivateUnwrapSecureMessageFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnData(const std::string& data);

  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.unwrapSecureMessage",
                             EASYUNLOCKPRIVATE_UNWRAPSECUREMESSAGE)

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateUnwrapSecureMessageFunction);
};

class EasyUnlockPrivateSetPermitAccessFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.setPermitAccess",
                             EASYUNLOCKPRIVATE_SETPERMITACCESS)
  EasyUnlockPrivateSetPermitAccessFunction();

 private:
  ~EasyUnlockPrivateSetPermitAccessFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateSetPermitAccessFunction);
};

class EasyUnlockPrivateGetPermitAccessFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.getPermitAccess",
                             EASYUNLOCKPRIVATE_GETPERMITACCESS)
  EasyUnlockPrivateGetPermitAccessFunction();

 protected:
  ~EasyUnlockPrivateGetPermitAccessFunction() override;

  // Writes the user's public and private key in base64 form to the
  // |user_public_key| and |user_private_key| fields. Exposed for testing.
  virtual void GetKeyPairForExperiment(std::string* user_public_key,
                                       std::string* user_private_key);

 private:
  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateGetPermitAccessFunction);
};

class EasyUnlockPrivateClearPermitAccessFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.clearPermitAccess",
                             EASYUNLOCKPRIVATE_CLEARPERMITACCESS)
  EasyUnlockPrivateClearPermitAccessFunction();

 private:
  ~EasyUnlockPrivateClearPermitAccessFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateClearPermitAccessFunction);
};

class EasyUnlockPrivateSetRemoteDevicesFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.setRemoteDevices",
                             EASYUNLOCKPRIVATE_SETREMOTEDEVICES)
  EasyUnlockPrivateSetRemoteDevicesFunction();

 private:
  ~EasyUnlockPrivateSetRemoteDevicesFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateSetRemoteDevicesFunction);
};

class EasyUnlockPrivateGetRemoteDevicesFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.getRemoteDevices",
                             EASYUNLOCKPRIVATE_GETREMOTEDEVICES)
  EasyUnlockPrivateGetRemoteDevicesFunction();

 protected:
  ~EasyUnlockPrivateGetRemoteDevicesFunction() override;

  // Returns the user's private key used for the native experiment.
  // Exposed for testing.
  virtual std::string GetUserPrivateKey();

  // Returns the user's unlock keys used for the native experiment.
  // Exposed for testing.
  virtual std::vector<cryptauth::ExternalDeviceInfo> GetUnlockKeys();

 private:
  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback when the PSK of a device is derived.
  void OnPSKDerivedForDevice(const cryptauth::ExternalDeviceInfo& device,
                             const std::string& persistent_symmetric_key);

  // The permit id of the user. Used for the native experiment.
  std::string permit_id_;

  // The expected number of devices to return. Used for the native experiment.
  size_t expected_devices_count_;

  // Working list of the devices to return. Used for the native experiment.
  std::unique_ptr<base::ListValue> remote_devices_;

  // Used to derive devices' PSK. Used for the native experiment.
  std::unique_ptr<cryptauth::SecureMessageDelegate>
      secure_message_delegate_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateGetRemoteDevicesFunction);
};

class EasyUnlockPrivateGetUserInfoFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.getUserInfo",
                             EASYUNLOCKPRIVATE_GETUSERINFO)
  EasyUnlockPrivateGetUserInfoFunction();

 private:
  ~EasyUnlockPrivateGetUserInfoFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateGetUserInfoFunction);
};

class EasyUnlockPrivateShowErrorBubbleFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.showErrorBubble",
                             EASYUNLOCKPRIVATE_SHOWERRORBUBBLE)
  EasyUnlockPrivateShowErrorBubbleFunction();

 private:
  ~EasyUnlockPrivateShowErrorBubbleFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateShowErrorBubbleFunction);
};

class EasyUnlockPrivateHideErrorBubbleFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.hideErrorBubble",
                             EASYUNLOCKPRIVATE_HIDEERRORBUBBLE)
  EasyUnlockPrivateHideErrorBubbleFunction();

 private:
  ~EasyUnlockPrivateHideErrorBubbleFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateHideErrorBubbleFunction);
};

class EasyUnlockPrivateFindSetupConnectionFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.findSetupConnection",
                             EASYUNLOCKPRIVATE_FINDSETUPCONNECTION)
  EasyUnlockPrivateFindSetupConnectionFunction();

 private:
  ~EasyUnlockPrivateFindSetupConnectionFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  // Called when the connection with the remote device advertising the setup
  // service was found.
  void OnConnectionFound(std::unique_ptr<cryptauth::Connection> connection);

  // Callback when waiting for |connection_finder_| to return.
  void OnConnectionFinderTimedOut();

  // The BLE connection finder instance.
  std::unique_ptr<proximity_auth::BluetoothLowEnergyConnectionFinder>
      connection_finder_;

  // Used for timing out when waiting for the connection finder to return.
  std::unique_ptr<base::Timer> timer_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateFindSetupConnectionFunction);
};

class EasyUnlockPrivateSetupConnectionDisconnectFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.setupConnectionDisconnect",
                             EASYUNLOCKPRIVATE_SETUPCONNECTIONDISCONNECT)
  EasyUnlockPrivateSetupConnectionDisconnectFunction();

 private:
  ~EasyUnlockPrivateSetupConnectionDisconnectFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateSetupConnectionDisconnectFunction);
};

class EasyUnlockPrivateSetupConnectionSendFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.setupConnectionSend",
                             EASYUNLOCKPRIVATE_SETUPCONNECTIONSEND)
  EasyUnlockPrivateSetupConnectionSendFunction();

 private:
  ~EasyUnlockPrivateSetupConnectionSendFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateSetupConnectionSendFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_API_H_
