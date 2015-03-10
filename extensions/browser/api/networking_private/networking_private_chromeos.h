// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_CHROMEOS_H_
#define EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_CHROMEOS_H_

#include "extensions/browser/api/networking_private/networking_private_delegate.h"

namespace context {
class BrowserContext;
}

namespace extensions {

// Chrome OS NetworkingPrivateDelegate implementation.

class NetworkingPrivateChromeOS : public NetworkingPrivateDelegate {
 public:
  // |verify_delegate| is passed to NetworkingPrivateDelegate and may be NULL.
  NetworkingPrivateChromeOS(content::BrowserContext* browser_context,
                            scoped_ptr<VerifyDelegate> verify_delegate);
  ~NetworkingPrivateChromeOS() override;

  // NetworkingPrivateApi
  void GetProperties(const std::string& guid,
                     const DictionaryCallback& success_callback,
                     const FailureCallback& failure_callback) override;
  void GetManagedProperties(const std::string& guid,
                            const DictionaryCallback& success_callback,
                            const FailureCallback& failure_callback) override;
  void GetState(const std::string& guid,
                const DictionaryCallback& success_callback,
                const FailureCallback& failure_callback) override;
  void SetProperties(const std::string& guid,
                     scoped_ptr<base::DictionaryValue> properties,
                     const VoidCallback& success_callback,
                     const FailureCallback& failure_callback) override;
  void CreateNetwork(bool shared,
                     scoped_ptr<base::DictionaryValue> properties,
                     const StringCallback& success_callback,
                     const FailureCallback& failure_callback) override;
  void GetNetworks(const std::string& network_type,
                   bool configured_only,
                   bool visible_only,
                   int limit,
                   const NetworkListCallback& success_callback,
                   const FailureCallback& failure_callback) override;
  void StartConnect(const std::string& guid,
                    const VoidCallback& success_callback,
                    const FailureCallback& failure_callback) override;
  void StartDisconnect(const std::string& guid,
                       const VoidCallback& success_callback,
                       const FailureCallback& failure_callback) override;
  void StartActivate(const std::string& guid,
                     const std::string& carrier,
                     const VoidCallback& success_callback,
                     const FailureCallback& failure_callback) override;
  void SetWifiTDLSEnabledState(
      const std::string& ip_or_mac_address,
      bool enabled,
      const StringCallback& success_callback,
      const FailureCallback& failure_callback) override;
  void GetWifiTDLSStatus(const std::string& ip_or_mac_address,
                         const StringCallback& success_callback,
                         const FailureCallback& failure_callback) override;
  void GetCaptivePortalStatus(const std::string& guid,
                              const StringCallback& success_callback,
                              const FailureCallback& failure_callback) override;
  scoped_ptr<base::ListValue> GetEnabledNetworkTypes() override;
  bool EnableNetworkType(const std::string& type) override;
  bool DisableNetworkType(const std::string& type) override;
  bool RequestScan() override;

 private:
  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateChromeOS);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_CHROMEOS_H_
