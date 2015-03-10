// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_DELEGATE_H_
#define EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_DELEGATE_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class NetworkingPrivateDelegateObserver;

namespace core_api {
namespace networking_private {
struct VerificationProperties;
}  // networking_private
}  // core_api

// Base class for platform dependent networkingPrivate API implementations.
// All inputs and results for this class use ONC values. See
// networking_private.json for descriptions of the expected inputs and results.
class NetworkingPrivateDelegate : public KeyedService {
 public:
  typedef base::Callback<void(scoped_ptr<base::DictionaryValue>)>
      DictionaryCallback;
  typedef base::Callback<void()> VoidCallback;
  typedef base::Callback<void(bool)> BoolCallback;
  typedef base::Callback<void(const std::string&)> StringCallback;
  typedef base::Callback<void(scoped_ptr<base::ListValue>)> NetworkListCallback;
  typedef base::Callback<void(const std::string&)> FailureCallback;
  typedef core_api::networking_private::VerificationProperties
      VerificationProperties;

  // The Verify* methods will be forwarded to a delegate implementation if
  // provided, otherwise they will fail. A separate delegate it used so that the
  // current Verify* implementations are not exposed outside of src/chrome.
  class VerifyDelegate {
   public:
    typedef NetworkingPrivateDelegate::VerificationProperties
        VerificationProperties;
    typedef NetworkingPrivateDelegate::BoolCallback BoolCallback;
    typedef NetworkingPrivateDelegate::StringCallback StringCallback;
    typedef NetworkingPrivateDelegate::FailureCallback FailureCallback;

    VerifyDelegate();
    virtual ~VerifyDelegate();

    virtual void VerifyDestination(
        const VerificationProperties& verification_properties,
        const BoolCallback& success_callback,
        const FailureCallback& failure_callback) = 0;
    virtual void VerifyAndEncryptCredentials(
        const std::string& guid,
        const VerificationProperties& verification_properties,
        const StringCallback& success_callback,
        const FailureCallback& failure_callback) = 0;
    virtual void VerifyAndEncryptData(
        const VerificationProperties& verification_properties,
        const std::string& data,
        const StringCallback& success_callback,
        const FailureCallback& failure_callback) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(VerifyDelegate);
  };

  // If |verify_delegate| is not NULL, the Verify* methods will be forwarded
  // to the delegate. Otherwise they will fail with a NotSupported error.
  explicit NetworkingPrivateDelegate(
      scoped_ptr<VerifyDelegate> verify_delegate);
  ~NetworkingPrivateDelegate() override;

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
                             scoped_ptr<base::DictionaryValue> properties,
                             const VoidCallback& success_callback,
                             const FailureCallback& failure_callback) = 0;
  virtual void CreateNetwork(bool shared,
                             scoped_ptr<base::DictionaryValue> properties,
                             const StringCallback& success_callback,
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

  // Synchronous methods

  // Returns a list of ONC type strings.
  virtual scoped_ptr<base::ListValue> GetEnabledNetworkTypes() = 0;

  // Returns true if the ONC network type |type| is enabled.
  virtual bool EnableNetworkType(const std::string& type) = 0;

  // Returns true if the ONC network type |type| is disabled.
  virtual bool DisableNetworkType(const std::string& type) = 0;

  // Returns true if a scan was requested. It may take many seconds for a scan
  // to complete. The scan may or may not trigger API events when complete.
  virtual bool RequestScan() = 0;

  // Optional methods for adding a NetworkingPrivateDelegateObserver for
  // implementations that require it (non-chromeos).
  virtual void AddObserver(NetworkingPrivateDelegateObserver* observer);
  virtual void RemoveObserver(NetworkingPrivateDelegateObserver* observer);

  // Verify* methods are forwarded to |verify_delegate_| if not NULL.
  void VerifyDestination(const VerificationProperties& verification_properties,
                         const BoolCallback& success_callback,
                         const FailureCallback& failure_callback);
  void VerifyAndEncryptCredentials(
      const std::string& guid,
      const VerificationProperties& verification_properties,
      const StringCallback& success_callback,
      const FailureCallback& failure_callback);
  void VerifyAndEncryptData(
      const VerificationProperties& verification_properties,
      const std::string& data,
      const StringCallback& success_callback,
      const FailureCallback& failure_callback);

 private:
  // Interface for Verify* methods. May be NULL.
  scoped_ptr<VerifyDelegate> verify_delegate_;

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateDelegate);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_DELEGATE_H_
