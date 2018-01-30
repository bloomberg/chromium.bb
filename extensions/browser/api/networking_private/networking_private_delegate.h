// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_DELEGATE_H_
#define EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/common/api/networking_private.h"

namespace extensions {

class NetworkingPrivateDelegateObserver;

// Base class for platform dependent networkingPrivate API implementations.
// All inputs and results for this class use ONC values. See
// networking_private.idl for descriptions of the expected inputs and results.
class NetworkingPrivateDelegate : public KeyedService {
 public:
  using DictionaryCallback =
      base::Callback<void(std::unique_ptr<base::DictionaryValue>)>;
  using VoidCallback = base::Callback<void()>;
  using BoolCallback = base::Callback<void(bool)>;
  using StringCallback = base::Callback<void(const std::string&)>;
  using NetworkListCallback =
      base::Callback<void(std::unique_ptr<base::ListValue>)>;
  using FailureCallback = base::Callback<void(const std::string&)>;
  using DeviceStateList = std::vector<
      std::unique_ptr<api::networking_private::DeviceStateProperties>>;

  // Delegate for forwarding UI requests, e.g. for showing the account UI.
  class UIDelegate {
   public:
    UIDelegate();
    virtual ~UIDelegate();

    // Navigate to the acoount details page for the cellular network associated
    // with |guid|.
    virtual void ShowAccountDetails(const std::string& guid) const = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(UIDelegate);
  };

  NetworkingPrivateDelegate();
  ~NetworkingPrivateDelegate() override;

  void set_ui_delegate(std::unique_ptr<UIDelegate> ui_delegate) {
    ui_delegate_ = std::move(ui_delegate);
  }

  const UIDelegate* ui_delegate() { return ui_delegate_.get(); }

  // Asynchronous methods
  virtual void GetProperties(const std::string& guid,
                             const DictionaryCallback& success_callback,
                             const FailureCallback& failure_callback) = 0;
  virtual void GetManagedProperties(
      const std::string& guid,
      const DictionaryCallback& success_callback,
      const FailureCallback& failure_callback) = 0;
  virtual void GetState(const std::string& guid,
                        const DictionaryCallback& success_callback,
                        const FailureCallback& failure_callback) = 0;
  virtual void SetProperties(const std::string& guid,
                             std::unique_ptr<base::DictionaryValue> properties,
                             bool allow_set_shared_config,
                             const VoidCallback& success_callback,
                             const FailureCallback& failure_callback) = 0;
  virtual void CreateNetwork(bool shared,
                             std::unique_ptr<base::DictionaryValue> properties,
                             const StringCallback& success_callback,
                             const FailureCallback& failure_callback) = 0;
  virtual void ForgetNetwork(const std::string& guid,
                             bool allow_forget_shared_config,
                             const VoidCallback& success_callback,
                             const FailureCallback& failure_callback) = 0;
  virtual void GetNetworks(const std::string& network_type,
                           bool configured_only,
                           bool visible_only,
                           int limit,
                           const NetworkListCallback& success_callback,
                           const FailureCallback& failure_callback) = 0;
  virtual void StartConnect(const std::string& guid,
                            const VoidCallback& success_callback,
                            const FailureCallback& failure_callback) = 0;
  virtual void StartDisconnect(const std::string& guid,
                               const VoidCallback& success_callback,
                               const FailureCallback& failure_callback) = 0;
  virtual void StartActivate(const std::string& guid,
                             const std::string& carrier,
                             const VoidCallback& success_callback,
                             const FailureCallback& failure_callback);
  virtual void SetWifiTDLSEnabledState(
      const std::string& ip_or_mac_address,
      bool enabled,
      const StringCallback& success_callback,
      const FailureCallback& failure_callback) = 0;
  virtual void GetWifiTDLSStatus(const std::string& ip_or_mac_address,
                                 const StringCallback& success_callback,
                                 const FailureCallback& failure_callback) = 0;
  virtual void GetCaptivePortalStatus(
      const std::string& guid,
      const StringCallback& success_callback,
      const FailureCallback& failure_callback) = 0;
  virtual void UnlockCellularSim(const std::string& guid,
                                 const std::string& pin,
                                 const std::string& puk,
                                 const VoidCallback& success_callback,
                                 const FailureCallback& failure_callback) = 0;
  virtual void SetCellularSimState(const std::string& guid,
                                   bool require_pin,
                                   const std::string& current_pin,
                                   const std::string& new_pin,
                                   const VoidCallback& success_callback,
                                   const FailureCallback& failure_callback) = 0;
  virtual void SelectCellularMobileNetwork(
      const std::string& guid,
      const std::string& network_id,
      const VoidCallback& success_callback,
      const FailureCallback& failure_callback) = 0;

  // Synchronous methods

  // Returns a list of ONC type strings.
  virtual std::unique_ptr<base::ListValue> GetEnabledNetworkTypes() = 0;

  // Returns a list of DeviceStateProperties.
  virtual std::unique_ptr<DeviceStateList> GetDeviceStateList() = 0;

  // Returns a dictionary of global policy values (may be empty). Note: the
  // dictionary is expected to be a superset of the networkingPrivate
  // GlobalPolicy dictionary. Any properties not in GlobalPolicy will be
  // ignored.
  virtual std::unique_ptr<base::DictionaryValue> GetGlobalPolicy() = 0;

  // Returns a dictionary of certificate lists.
  virtual std::unique_ptr<base::DictionaryValue> GetCertificateLists() = 0;

  // Returns true if the ONC network type |type| is enabled.
  virtual bool EnableNetworkType(const std::string& type) = 0;

  // Returns true if the ONC network type |type| is disabled.
  virtual bool DisableNetworkType(const std::string& type) = 0;

  // Returns true if a scan was requested. It may take many seconds for a scan
  // to complete. The scan may or may not trigger API events when complete.
  // |type| is the type of network to request a scan for; if empty, scans for
  // all supported network types except Cellular, which must be requested
  // explicitly.
  virtual bool RequestScan(const std::string& type) = 0;

  // Optional methods for adding a NetworkingPrivateDelegateObserver for
  // implementations that require it (non-chromeos).
  virtual void AddObserver(NetworkingPrivateDelegateObserver* observer);
  virtual void RemoveObserver(NetworkingPrivateDelegateObserver* observer);

 private:
  // Interface for UI methods. May be null.
  std::unique_ptr<UIDelegate> ui_delegate_;

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateDelegate);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_DELEGATE_H_
