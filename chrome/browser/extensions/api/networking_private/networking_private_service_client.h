// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_SERVICE_CLIENT_H_
#define CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_SERVICE_CLIENT_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/id_map.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "base/supports_user_data.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_delegate.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/wifi/wifi_service.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"
#include "net/base/network_change_notifier.h"

namespace base {
class SequencedTaskRunner;
}

namespace wifi {
class WiFiService;
}

namespace extensions {

using wifi::WiFiService;

// Windows / Mac NetworkingPrivateApi implementation. This implements
// WiFiService and CryptoVerify interfaces and invokes them on the worker
// thread. Observes |OnNetworkChanged| notifications and posts them to
// WiFiService on the worker thread to |UpdateConnectedNetwork|. Created and
// called from the UI thread.
class NetworkingPrivateServiceClient
    : public KeyedService,
      public NetworkingPrivateDelegate,
      net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  // Interface for Verify* methods implementation.
  class CryptoVerify {
   public:
    typedef base::Callback<
        void(const std::string& key_data, const std::string& error)>
        VerifyAndEncryptCredentialsCallback;

    // TODO(stevenjb): Remove this when addressing crbug.com/394481
    struct Credentials {
      Credentials();
      ~Credentials();
      std::string certificate;
      std::string signed_data;
      std::string unsigned_data;
      std::string device_bssid;
      std::string public_key;
    };

    CryptoVerify();
    virtual ~CryptoVerify();

    // Must be provided by the implementation. May return NULL if certificate
    // verification is unavailable, see NetworkingPrivateServiceClient().
    static CryptoVerify* Create();

    virtual void VerifyDestination(const Credentials& credentials,
                                   bool* verified,
                                   std::string* error) = 0;

    virtual void VerifyAndEncryptCredentials(
        const std::string& network_guid,
        const Credentials& credentials,
        const VerifyAndEncryptCredentialsCallback& callback) = 0;

    virtual void VerifyAndEncryptData(const Credentials& credentials,
                                      const std::string& data,
                                      std::string* base64_encoded_ciphertext,
                                      std::string* error) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(CryptoVerify);
  };

  // Interface for observing Network Events.
  class Observer {
   public:
    Observer() {}
    virtual ~Observer() {}

    virtual void OnNetworksChangedEvent(
        const std::vector<std::string>& network_guids) = 0;
    virtual void OnNetworkListChangedEvent(
        const std::vector<std::string>& network_guids) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  // Takes ownership of |wifi_service| and |crypto_verify|. They are accessed
  // and deleted on the worker thread. The deletion task is posted during the
  // NetworkingPrivateServiceClient shutdown. |crypto_verify| may be NULL in
  // which case Verify* will return a 'not implemented' error.
  NetworkingPrivateServiceClient(wifi::WiFiService* wifi_service,
                                 CryptoVerify* crypto_verify);

  // KeyedService
  virtual void Shutdown() OVERRIDE;

  // NetworkingPrivateDelegate
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
                             scoped_ptr<base::DictionaryValue> properties_dict,
                             const VoidCallback& success_callback,
                             const FailureCallback& failure_callback) OVERRIDE;
  virtual void CreateNetwork(bool shared,
                             scoped_ptr<base::DictionaryValue> properties_dict,
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

  // Adds observer to network events.
  void AddObserver(Observer* network_events_observer);

  // Removes observer to network events. If there is no observers,
  // then process can be shut down when there are no more calls pending return.
  void RemoveObserver(Observer* network_events_observer);

  // NetworkChangeNotifier::NetworkChangeObserver implementation.
  virtual void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) OVERRIDE;

 private:
  // Callbacks to extension api function objects. Keep reference to API object
  // and are released in ShutdownOnUIThread. Run when WiFiService calls back
  // into NetworkingPrivateServiceClient's callback wrappers.
  typedef int32 ServiceCallbacksID;
  struct ServiceCallbacks {
    ServiceCallbacks();
    ~ServiceCallbacks();

    DictionaryCallback get_properties_callback;
    VoidCallback start_connect_callback;
    VoidCallback start_disconnect_callback;
    VoidCallback set_properties_callback;
    StringCallback create_network_callback;
    NetworkListCallback get_visible_networks_callback;
    FailureCallback failure_callback;

    BoolCallback verify_destination_callback;
    StringCallback verify_and_encrypt_data_callback;
    StringCallback verify_and_encrypt_credentials_callback;

    ServiceCallbacksID id;
  };
  typedef IDMap<ServiceCallbacks, IDMapOwnPointer> ServiceCallbacksMap;

  virtual ~NetworkingPrivateServiceClient();

  // Callback wrappers.
  void AfterGetProperties(ServiceCallbacksID callback_id,
                          const std::string& network_guid,
                          scoped_ptr<base::DictionaryValue> properties,
                          const std::string* error);
  void AfterSetProperties(ServiceCallbacksID callback_id,
                          const std::string* error);
  void AfterCreateNetwork(ServiceCallbacksID callback_id,
                          const std::string* network_guid,
                          const std::string* error);
  void AfterGetVisibleNetworks(ServiceCallbacksID callback_id,
                               scoped_ptr<base::ListValue> networks);
  void AfterStartConnect(ServiceCallbacksID callback_id,
                         const std::string* error);
  void AfterStartDisconnect(ServiceCallbacksID callback_id,
                            const std::string* error);
  void AfterVerifyDestination(ServiceCallbacksID callback_id,
                              const bool* result,
                              const std::string* error);
  void AfterVerifyAndEncryptData(ServiceCallbacksID callback_id,
                                 const std::string* result,
                                 const std::string* error);
  void AfterVerifyAndEncryptCredentials(ServiceCallbacksID callback_id,
                                        const std::string& encrypted_data,
                                        const std::string& error);

  void OnNetworksChangedEventOnUIThread(
      const WiFiService::NetworkGuidList& network_guid_list);
  void OnNetworkListChangedEventOnUIThread(
      const WiFiService::NetworkGuidList& network_guid_list);

  // Add new |ServiceCallbacks| to |callbacks_map_|.
  ServiceCallbacks* AddServiceCallbacks();
  // Removes ServiceCallbacks for |callback_id| from |callbacks_map_|.
  void RemoveServiceCallbacks(ServiceCallbacksID callback_id);

  // Callbacks to run when callback is called from WiFiService.
  ServiceCallbacksMap callbacks_map_;
  // Observers to Network Events.
  ObserverList<Observer> network_events_observers_;
  // Interface for Verify* methods. Used and deleted on the worker thread.
  // May be NULL.
  scoped_ptr<CryptoVerify> crypto_verify_;
  // Interface to WiFiService. Used and deleted on the worker thread.
  scoped_ptr<wifi::WiFiService> wifi_service_;
  // Sequence token associated with wifi tasks.
  base::SequencedWorkerPool::SequenceToken sequence_token_;
  // Task runner for worker tasks.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  // Use WeakPtrs for callbacks from |wifi_service_| and |crypto_verify_|.
  base::WeakPtrFactory<NetworkingPrivateServiceClient> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateServiceClient);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_SERVICE_CLIENT_H_
