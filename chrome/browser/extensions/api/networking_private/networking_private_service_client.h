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
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
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

// The client wrapper for the WiFiService and CryptoVerify interfaces to invoke
// them on worker thread. Observes |OnNetworkChanged| notifications and posts
// them to WiFiService on worker thread to |UpdateConnectedNetwork|. Always used
// from UI thread only.
class NetworkingPrivateServiceClient
    : public BrowserContextKeyedService,
      net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  // Interface for Verify* methods implementation.
  class CryptoVerify {
   public:
    CryptoVerify() {}
    virtual ~CryptoVerify() {}

    static CryptoVerify* Create();

    virtual void VerifyDestination(scoped_ptr<base::ListValue> args,
                                   bool* verified,
                                   std::string* error) = 0;

    virtual void VerifyAndEncryptData(scoped_ptr<base::ListValue> args,
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

  // An error callback used by most of API functions to receive error results
  // from the NetworkingPrivateServiceClient.
  typedef base::Callback<
      void(const std::string& error_name,
           scoped_ptr<base::DictionaryValue> error_data)> ErrorCallback;

  // An error callback used by most of Crypto Verify* API functions to receive
  // error results from the NetworkingPrivateServiceClient.
  // TODO(mef): Cleanup networking_private_api.* to make consistent
  // NetworkingPrivateXXXFunction naming and error callbacks.
  typedef base::Callback<
      void(const std::string& error_name,
           const std::string& error)> CryptoErrorCallback;

  // Callback used to return bool result from VerifyDestination function.
  typedef base::Callback<void(bool result)> BoolResultCallback;

  // Callback used to return string result from VerifyAndEncryptData function.
  typedef base::Callback<void(const std::string& result)> StringResultCallback;

  // Callback used to return Dictionary of network properties.
  typedef base::Callback<
      void(const std::string& network_guid,
           const base::DictionaryValue& dictionary)> DictionaryResultCallback;

  // Callback used to return List of visibile networks.
  typedef base::Callback<
      void(const base::ListValue& network_list)> ListResultCallback;

  // Takes ownership of |wifi_service| and |crypto_verify|. They are accessed
  // and deleted on the worker thread. The deletion task is posted during the
  // NetworkingPrivateServiceClient shutdown.
  NetworkingPrivateServiceClient(wifi::WiFiService* wifi_service,
                                 CryptoVerify* crypto_verify);

  // BrowserContextKeyedServices method override.
  virtual void Shutdown() OVERRIDE;

  // Gets the properties of the network with id |network_guid|. See note on
  // |callback| and |error_callback|, in class description above.
  void GetProperties(const std::string& network_guid,
                     const DictionaryResultCallback& callback,
                     const ErrorCallback& error_callback);

  // Gets the merged properties of the network with id |network_guid| from these
  // sources: User settings, shared settings, user policy, device policy and
  // the currently active settings. See note on
  // |callback| and |error_callback|, in class description above.
  void GetManagedProperties(const std::string& network_guid,
                            const DictionaryResultCallback& callback,
                            const ErrorCallback& error_callback);

  // Gets the cached read-only properties of the network with id |network_guid|.
  // This is meant to be a higher performance function than |GetProperties|,
  // which requires a round trip to query the networking subsystem. It only
  // returns a subset of the properties returned by |GetProperties|. See note on
  // |callback| and |error_callback|, in class description above.
  void GetState(const std::string& network_guid,
                const DictionaryResultCallback& callback,
                const ErrorCallback& error_callback);

  // Start connect to the network with id |network_guid|. See note on
  // |callback| and |error_callback|, in class description above.
  void StartConnect(const std::string& network_guid,
                    const base::Closure& callback,
                    const ErrorCallback& error_callback);

  // Start disconnect from the network with id |network_guid|. See note on
  // |callback| and |error_callback|, in class description above.
  void StartDisconnect(const std::string& network_guid,
                       const base::Closure& callback,
                       const ErrorCallback& error_callback);

  // Sets the |properties| of the network with id |network_guid|. See note on
  // |callback| and |error_callback|, in class description above.
  void SetProperties(const std::string& network_guid,
                     const base::DictionaryValue& properties,
                     const base::Closure& callback,
                     const ErrorCallback& error_callback);

  // Creates a new network configuration from |properties|. If |shared| is true,
  // share this network configuration with other users. If a matching configured
  // network already exists, this will fail. On success invokes |callback| with
  // the |network_guid| of the new network. See note on |callback| and
  // error_callback|, in class description above.
  void CreateNetwork(bool shared,
                     const base::DictionaryValue& properties,
                     const StringResultCallback& callback,
                     const ErrorCallback& error_callback);

  // Requests network scan. Broadcasts NetworkListChangedEvent upon completion.
  void RequestNetworkScan();

  // Gets the list of visible networks of |network_type| and calls |callback|.
  void GetVisibleNetworks(const std::string& network_type,
                          const ListResultCallback& callback);

  // Verify that Chromecast provides valid cryptographically signed properties.
  void VerifyDestination(scoped_ptr<base::ListValue> args,
                         const BoolResultCallback& callback,
                         const CryptoErrorCallback& error_callback);

  // Verify that Chromecast provides valid cryptographically signed properties.
  // If valid, then encrypt data using Chromecast's public key.
  void VerifyAndEncryptData(scoped_ptr<base::ListValue> args,
                            const StringResultCallback& callback,
                            const CryptoErrorCallback& error_callback);

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

    DictionaryResultCallback get_properties_callback;
    base::Closure start_connect_callback;
    base::Closure start_disconnect_callback;
    base::Closure set_properties_callback;
    StringResultCallback create_network_callback;
    ListResultCallback get_visible_networks_callback;
    ErrorCallback error_callback;

    BoolResultCallback verify_destination_callback;
    StringResultCallback verify_and_encrypt_data_callback;
    CryptoErrorCallback crypto_error_callback;

    ServiceCallbacksID id;
  };
  typedef IDMap<ServiceCallbacks, IDMapOwnPointer> ServiceCallbacksMap;

  virtual ~NetworkingPrivateServiceClient();

  // Callback wrappers.
  void AfterGetProperties(ServiceCallbacksID callback_id,
                          const std::string& network_guid,
                          const base::DictionaryValue* properties,
                          const std::string* error);
  void AfterSetProperties(ServiceCallbacksID callback_id,
                          const std::string* error);
  void AfterCreateNetwork(ServiceCallbacksID callback_id,
                          const std::string* network_guid,
                          const std::string* error);
  void AfterGetVisibleNetworks(ServiceCallbacksID callback_id,
                               const base::ListValue* network_list);
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
