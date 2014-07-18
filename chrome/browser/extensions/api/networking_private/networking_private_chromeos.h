// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_CHROMEOS_H_
#define CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_CHROMEOS_H_

#include "chrome/browser/extensions/api/networking_private/networking_private_delegate.h"
#include "components/keyed_service/core/keyed_service.h"

namespace context {
class BrowserContext;
}

namespace extensions {

// Chrome OS NetworkingPrivateDelegate implementation.

class NetworkingPrivateChromeOS : public KeyedService,
                                  public NetworkingPrivateDelegate {
 public:
  explicit NetworkingPrivateChromeOS(content::BrowserContext* browser_context);

  // NetworkingPrivateApi
  virtual void GetProperties(const std::string& guid,
                             const DictionaryCallback& success_callback,
                             const FailureCallback& failure_callback) OVERRIDE;
  virtual void GetManagedProperties(
      const std::string& guid,
      const DictionaryCallback& success_callback,
      const FailureCallback& failure_callback) OVERRIDE;
  virtual void GetState(const std::string& guid,
                        const DictionaryCallback& success_callback,
                        const FailureCallback& failure_callback) OVERRIDE;
  virtual void SetProperties(const std::string& guid,
                             scoped_ptr<base::DictionaryValue> properties,
                             const VoidCallback& success_callback,
                             const FailureCallback& failure_callback) OVERRIDE;
  virtual void CreateNetwork(bool shared,
                             scoped_ptr<base::DictionaryValue> properties,
                             const StringCallback& success_callback,
                             const FailureCallback& failure_callback) OVERRIDE;
  virtual void GetNetworks(const std::string& network_type,
                           bool configured_only,
                           bool visible_only,
                           int limit,
                           const NetworkListCallback& success_callback,
                           const FailureCallback& failure_callback) OVERRIDE;
  virtual void StartConnect(const std::string& guid,
                            const VoidCallback& success_callback,
                            const FailureCallback& failure_callback) OVERRIDE;
  virtual void StartDisconnect(
      const std::string& guid,
      const VoidCallback& success_callback,
      const FailureCallback& failure_callback) OVERRIDE;
  virtual void VerifyDestination(
      const VerificationProperties& verification_properties,
      const BoolCallback& success_callback,
      const FailureCallback& failure_callback) OVERRIDE;
  virtual void VerifyAndEncryptCredentials(
      const std::string& guid,
      const VerificationProperties& verification_properties,
      const StringCallback& success_callback,
      const FailureCallback& failure_callback) OVERRIDE;
  virtual void VerifyAndEncryptData(
      const VerificationProperties& verification_properties,
      const std::string& data,
      const StringCallback& success_callback,
      const FailureCallback& failure_callback) OVERRIDE;
  virtual void SetWifiTDLSEnabledState(
      const std::string& ip_or_mac_address,
      bool enabled,
      const StringCallback& success_callback,
      const FailureCallback& failure_callback) OVERRIDE;
  virtual void GetWifiTDLSStatus(
      const std::string& ip_or_mac_address,
      const StringCallback& success_callback,
      const FailureCallback& failure_callback) OVERRIDE;
  virtual void GetCaptivePortalStatus(
      const std::string& guid,
      const StringCallback& success_callback,
      const FailureCallback& failure_callback) OVERRIDE;
  virtual scoped_ptr<base::ListValue> GetEnabledNetworkTypes() OVERRIDE;
  virtual bool EnableNetworkType(const std::string& type) OVERRIDE;
  virtual bool DisableNetworkType(const std::string& type) OVERRIDE;
  virtual bool RequestScan() OVERRIDE;

 private:
  virtual ~NetworkingPrivateChromeOS();

  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateChromeOS);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_CHROMEOS_H_
