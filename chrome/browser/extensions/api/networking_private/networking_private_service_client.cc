// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/networking_private/networking_private_service_client.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/worker_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_api.h"
#include "chrome/common/extensions/api/networking_private.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

namespace {

const char kNetworkingPrivateSequenceTokenName[] = "NetworkingPrivate";

bool GetVerificationCredentials(
    const NetworkingPrivateDelegate::VerificationProperties& properties,
    NetworkingPrivateServiceClient::CryptoVerify::Credentials* credentials) {
  std::vector<std::string> data_parts;
  data_parts.push_back(properties.device_ssid);
  data_parts.push_back(properties.device_serial);
  data_parts.push_back(properties.device_bssid);
  data_parts.push_back(properties.public_key);
  data_parts.push_back(properties.nonce);
  credentials->unsigned_data = JoinString(data_parts, ",");
  if (!base::Base64Decode(properties.signed_data, &credentials->signed_data)) {
    LOG(ERROR) << "Failed to decode signed data: " << properties.signed_data;
    return false;
  }
  credentials->certificate = properties.certificate;
  credentials->device_bssid = properties.device_bssid;
  if (!base::Base64Decode(properties.public_key, &credentials->public_key)) {
    LOG(ERROR) << "Failed to decode public key";
    return false;
  }
  return true;
}

// Deletes WiFiService and CryptoVerify objects on worker thread.
void ShutdownServicesOnWorkerThread(
    scoped_ptr<wifi::WiFiService> wifi_service,
    scoped_ptr<NetworkingPrivateServiceClient::CryptoVerify> crypto_verify) {
  DCHECK(wifi_service.get());
  DCHECK(crypto_verify.get());
}

// Forwards call back from VerifyAndEncryptCredentials on random thread to
// |callback| on correct |callback_loop_proxy|.
void AfterVerifyAndEncryptCredentialsRelay(
    const NetworkingPrivateServiceClient::CryptoVerify::
        VerifyAndEncryptCredentialsCallback& callback,
    scoped_refptr<base::MessageLoopProxy> callback_loop_proxy,
    const std::string& key_data,
    const std::string& error) {
  callback_loop_proxy->PostTask(FROM_HERE,
                                base::Bind(callback, key_data, error));
}

}  // namespace

NetworkingPrivateServiceClient::CryptoVerify::CryptoVerify() {}
NetworkingPrivateServiceClient::CryptoVerify::~CryptoVerify() {}

NetworkingPrivateServiceClient::CryptoVerify::Credentials::Credentials() {}
NetworkingPrivateServiceClient::CryptoVerify::Credentials::~Credentials() {}

NetworkingPrivateServiceClient::NetworkingPrivateServiceClient(
    wifi::WiFiService* wifi_service,
    CryptoVerify* crypto_verify)
    : crypto_verify_(crypto_verify),
      wifi_service_(wifi_service),
      weak_factory_(this) {
  sequence_token_ = BrowserThread::GetBlockingPool()->
      GetNamedSequenceToken(kNetworkingPrivateSequenceTokenName);
  task_runner_ = BrowserThread::GetBlockingPool()->
      GetSequencedTaskRunnerWithShutdownBehavior(
          sequence_token_,
          base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);
  task_runner_->PostTask(
    FROM_HERE,
    base::Bind(
        &WiFiService::Initialize,
        base::Unretained(wifi_service_.get()),
        task_runner_));
  task_runner_->PostTask(
    FROM_HERE,
    base::Bind(
        &WiFiService::SetEventObservers,
        base::Unretained(wifi_service_.get()),
        base::MessageLoopProxy::current(),
        base::Bind(
            &NetworkingPrivateServiceClient::OnNetworksChangedEventOnUIThread,
            weak_factory_.GetWeakPtr()),
        base::Bind(
            &NetworkingPrivateServiceClient::
                OnNetworkListChangedEventOnUIThread,
            weak_factory_.GetWeakPtr())));
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
}

NetworkingPrivateServiceClient::~NetworkingPrivateServiceClient() {
  // Verify that these objects were passed to ShutdownServicesOnWorkerThread to
  // be deleted after completion of all posted tasks.
  DCHECK(!wifi_service_.get());
  DCHECK(!crypto_verify_.get());
}

NetworkingPrivateServiceClient::ServiceCallbacks::ServiceCallbacks() {}

NetworkingPrivateServiceClient::ServiceCallbacks::~ServiceCallbacks() {}

void NetworkingPrivateServiceClient::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  // Clear callbacks map to release callbacks from UI thread.
  callbacks_map_.Clear();
  // Post ShutdownServicesOnWorkerThread task to delete services when all posted
  // tasks are done.
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ShutdownServicesOnWorkerThread,
                 base::Passed(&wifi_service_),
                 base::Passed(&crypto_verify_)));
}

void NetworkingPrivateServiceClient::AddObserver(Observer* observer) {
  network_events_observers_.AddObserver(observer);
}

void NetworkingPrivateServiceClient::RemoveObserver(Observer* observer) {
  network_events_observers_.RemoveObserver(observer);
}

void NetworkingPrivateServiceClient::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&WiFiService::RequestConnectedNetworkUpdate,
                 base::Unretained(wifi_service_.get())));
}

NetworkingPrivateServiceClient::ServiceCallbacks*
NetworkingPrivateServiceClient::AddServiceCallbacks() {
  ServiceCallbacks* service_callbacks = new ServiceCallbacks();
  service_callbacks->id = callbacks_map_.Add(service_callbacks);
  return service_callbacks;
}

void NetworkingPrivateServiceClient::RemoveServiceCallbacks(
    ServiceCallbacksID callback_id) {
  callbacks_map_.Remove(callback_id);
}

// NetworkingPrivateServiceClient implementation

void NetworkingPrivateServiceClient::GetProperties(
    const std::string& guid,
    const DictionaryCallback& success_callback,
    const FailureCallback& failure_callback) {
  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->failure_callback = failure_callback;
  service_callbacks->get_properties_callback = success_callback;

  scoped_ptr<base::DictionaryValue> properties(new base::DictionaryValue);
  std::string* error = new std::string;

  base::DictionaryValue* properties_ptr = properties.get();
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&WiFiService::GetProperties,
                 base::Unretained(wifi_service_.get()),
                 guid,
                 properties_ptr,
                 error),
      base::Bind(&NetworkingPrivateServiceClient::AfterGetProperties,
                 weak_factory_.GetWeakPtr(),
                 service_callbacks->id,
                 guid,
                 base::Passed(&properties),
                 base::Owned(error)));
}

void NetworkingPrivateServiceClient::GetManagedProperties(
    const std::string& guid,
    const DictionaryCallback& success_callback,
    const FailureCallback& failure_callback) {
  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->failure_callback = failure_callback;
  service_callbacks->get_properties_callback = success_callback;

  scoped_ptr<base::DictionaryValue> properties(new base::DictionaryValue);
  std::string* error = new std::string;

  base::DictionaryValue* properties_ptr = properties.get();
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&WiFiService::GetManagedProperties,
                 base::Unretained(wifi_service_.get()),
                 guid,
                 properties_ptr,
                 error),
      base::Bind(&NetworkingPrivateServiceClient::AfterGetProperties,
                 weak_factory_.GetWeakPtr(),
                 service_callbacks->id,
                 guid,
                 base::Passed(&properties),
                 base::Owned(error)));
}

void NetworkingPrivateServiceClient::GetState(
    const std::string& guid,
    const DictionaryCallback& success_callback,
    const FailureCallback& failure_callback) {
  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->failure_callback = failure_callback;
  service_callbacks->get_properties_callback = success_callback;

  scoped_ptr<base::DictionaryValue> properties(new base::DictionaryValue);
  std::string* error = new std::string;

  base::DictionaryValue* properties_ptr = properties.get();
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&WiFiService::GetState,
                 base::Unretained(wifi_service_.get()),
                 guid,
                 properties_ptr,
                 error),
      base::Bind(&NetworkingPrivateServiceClient::AfterGetProperties,
                 weak_factory_.GetWeakPtr(),
                 service_callbacks->id,
                 guid,
                 base::Passed(&properties),
                 base::Owned(error)));
}

void NetworkingPrivateServiceClient::SetProperties(
    const std::string& guid,
    scoped_ptr<base::DictionaryValue> properties,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback) {
  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->failure_callback = failure_callback;
  service_callbacks->set_properties_callback = success_callback;

  std::string* error = new std::string;

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&WiFiService::SetProperties,
                 base::Unretained(wifi_service_.get()),
                 guid,
                 base::Passed(&properties),
                 error),
      base::Bind(&NetworkingPrivateServiceClient::AfterSetProperties,
                 weak_factory_.GetWeakPtr(),
                 service_callbacks->id,
                 base::Owned(error)));
}

void NetworkingPrivateServiceClient::CreateNetwork(
    bool shared,
    scoped_ptr<base::DictionaryValue> properties,
    const StringCallback& success_callback,
    const FailureCallback& failure_callback) {
  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->failure_callback = failure_callback;
  service_callbacks->create_network_callback = success_callback;

  std::string* network_guid = new std::string;
  std::string* error = new std::string;

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&WiFiService::CreateNetwork,
                 base::Unretained(wifi_service_.get()),
                 shared,
                 base::Passed(&properties),
                 network_guid,
                 error),
      base::Bind(&NetworkingPrivateServiceClient::AfterCreateNetwork,
                 weak_factory_.GetWeakPtr(),
                 service_callbacks->id,
                 base::Owned(network_guid),
                 base::Owned(error)));
}

void NetworkingPrivateServiceClient::GetNetworks(
    const std::string& network_type,
    bool configured_only,
    bool visible_only,
    int limit,
    const NetworkListCallback& success_callback,
    const FailureCallback& failure_callback) {
  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->failure_callback = failure_callback;
  service_callbacks->get_visible_networks_callback = success_callback;

  scoped_ptr<base::ListValue> networks(new base::ListValue);

  // TODO(stevenjb/mef): Apply filters (configured, visible, limit).

  base::ListValue* networks_ptr = networks.get();
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&WiFiService::GetVisibleNetworks,
                 base::Unretained(wifi_service_.get()),
                 network_type,
                 networks_ptr,
                 false),
      base::Bind(&NetworkingPrivateServiceClient::AfterGetVisibleNetworks,
                 weak_factory_.GetWeakPtr(),
                 service_callbacks->id,
                 base::Passed(&networks)));
}

void NetworkingPrivateServiceClient::StartConnect(
    const std::string& guid,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback) {
  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->failure_callback = failure_callback;
  service_callbacks->start_connect_callback = success_callback;

  std::string* error = new std::string;

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&WiFiService::StartConnect,
                 base::Unretained(wifi_service_.get()),
                 guid,
                 error),
      base::Bind(&NetworkingPrivateServiceClient::AfterStartConnect,
                 weak_factory_.GetWeakPtr(),
                 service_callbacks->id,
                 base::Owned(error)));
}

void NetworkingPrivateServiceClient::StartDisconnect(
    const std::string& guid,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback) {
  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->failure_callback = failure_callback;
  service_callbacks->start_disconnect_callback = success_callback;

  std::string* error = new std::string;

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&WiFiService::StartDisconnect,
                 base::Unretained(wifi_service_.get()),
                 guid,
                 error),
      base::Bind(&NetworkingPrivateServiceClient::AfterStartDisconnect,
                 weak_factory_.GetWeakPtr(),
                 service_callbacks->id,
                 base::Owned(error)));
}

void NetworkingPrivateServiceClient::VerifyDestination(
    const VerificationProperties& verification_properties,
    const BoolCallback& success_callback,
    const FailureCallback& failure_callback) {
  if (!crypto_verify_) {
    failure_callback.Run(networking_private::kErrorNotSupported);
    return;
  }

  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->failure_callback = failure_callback;
  service_callbacks->verify_destination_callback = success_callback;

  CryptoVerify::Credentials credentials;
  if (!GetVerificationCredentials(verification_properties, &credentials)) {
    failure_callback.Run(networking_private::kErrorEncryptionError);
    return;
  }

  bool* result = new bool;
  std::string* error = new std::string;

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&CryptoVerify::VerifyDestination,
                 base::Unretained(crypto_verify_.get()),
                 credentials,
                 result,
                 error),
      base::Bind(&NetworkingPrivateServiceClient::AfterVerifyDestination,
                 weak_factory_.GetWeakPtr(),
                 service_callbacks->id,
                 base::Owned(result),
                 base::Owned(error)));
}

void NetworkingPrivateServiceClient::VerifyAndEncryptCredentials(
    const std::string& guid,
    const VerificationProperties& verification_properties,
    const StringCallback& success_callback,
    const FailureCallback& failure_callback) {
  if (!crypto_verify_) {
    failure_callback.Run(networking_private::kErrorNotSupported);
    return;
  }

  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->failure_callback = failure_callback;
  service_callbacks->verify_and_encrypt_credentials_callback = success_callback;

  CryptoVerify::Credentials credentials;
  if (!GetVerificationCredentials(verification_properties, &credentials)) {
    failure_callback.Run(networking_private::kErrorEncryptionError);
    return;
  }

  CryptoVerify::VerifyAndEncryptCredentialsCallback callback_relay(base::Bind(
      &AfterVerifyAndEncryptCredentialsRelay,
      base::Bind(
          &NetworkingPrivateServiceClient::AfterVerifyAndEncryptCredentials,
          weak_factory_.GetWeakPtr(),
          service_callbacks->id),
      base::MessageLoopProxy::current()));

  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&CryptoVerify::VerifyAndEncryptCredentials,
                                    base::Unretained(crypto_verify_.get()),
                                    guid,
                                    credentials,
                                    callback_relay));
}

void NetworkingPrivateServiceClient::VerifyAndEncryptData(
    const VerificationProperties& verification_properties,
    const std::string& data,
    const StringCallback& success_callback,
    const FailureCallback& failure_callback) {
  if (!crypto_verify_) {
    failure_callback.Run(networking_private::kErrorNotSupported);
    return;
  }

  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->failure_callback = failure_callback;
  service_callbacks->verify_and_encrypt_data_callback = success_callback;

  CryptoVerify::Credentials credentials;
  if (!GetVerificationCredentials(verification_properties, &credentials)) {
    failure_callback.Run(networking_private::kErrorEncryptionError);
    return;
  }

  std::string* result = new std::string;
  std::string* error = new std::string;

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&CryptoVerify::VerifyAndEncryptData,
                 base::Unretained(crypto_verify_.get()),
                 credentials,
                 data,
                 result,
                 error),
      base::Bind(&NetworkingPrivateServiceClient::AfterVerifyAndEncryptData,
                 weak_factory_.GetWeakPtr(),
                 service_callbacks->id,
                 base::Owned(result),
                 base::Owned(error)));
}

void NetworkingPrivateServiceClient::SetWifiTDLSEnabledState(
    const std::string& ip_or_mac_address,
    bool enabled,
    const StringCallback& success_callback,
    const FailureCallback& failure_callback) {
  failure_callback.Run(networking_private::kErrorNotSupported);
}

void NetworkingPrivateServiceClient::GetWifiTDLSStatus(
    const std::string& ip_or_mac_address,
    const StringCallback& success_callback,
    const FailureCallback& failure_callback) {
  failure_callback.Run(networking_private::kErrorNotSupported);
}

void NetworkingPrivateServiceClient::GetCaptivePortalStatus(
    const std::string& guid,
    const StringCallback& success_callback,
    const FailureCallback& failure_callback) {
  failure_callback.Run(networking_private::kErrorNotSupported);
}

scoped_ptr<base::ListValue>
NetworkingPrivateServiceClient::GetEnabledNetworkTypes() {
  scoped_ptr<base::ListValue> network_list;
  return network_list.Pass();
}

bool NetworkingPrivateServiceClient::EnableNetworkType(
    const std::string& type) {
  return false;
}

bool NetworkingPrivateServiceClient::DisableNetworkType(
    const std::string& type) {
  return false;
}

bool NetworkingPrivateServiceClient::RequestScan() {
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&WiFiService::RequestNetworkScan,
                 base::Unretained(wifi_service_.get())));
  return true;
}

////////////////////////////////////////////////////////////////////////////////

void NetworkingPrivateServiceClient::AfterGetProperties(
    ServiceCallbacksID callback_id,
    const std::string& network_guid,
    scoped_ptr<base::DictionaryValue> properties,
    const std::string* error) {
  ServiceCallbacks* service_callbacks = callbacks_map_.Lookup(callback_id);
  DCHECK(service_callbacks);
  if (!error->empty()) {
    DCHECK(!service_callbacks->failure_callback.is_null());
    service_callbacks->failure_callback.Run(*error);
  } else {
    DCHECK(!service_callbacks->get_properties_callback.is_null());
    service_callbacks->get_properties_callback.Run(properties.Pass());
  }
  RemoveServiceCallbacks(callback_id);
}

void NetworkingPrivateServiceClient::AfterGetVisibleNetworks(
    ServiceCallbacksID callback_id,
    scoped_ptr<base::ListValue> networks) {
  ServiceCallbacks* service_callbacks = callbacks_map_.Lookup(callback_id);
  DCHECK(service_callbacks);
  DCHECK(!service_callbacks->get_visible_networks_callback.is_null());
  service_callbacks->get_visible_networks_callback.Run(networks.Pass());
  RemoveServiceCallbacks(callback_id);
}

void NetworkingPrivateServiceClient::AfterSetProperties(
    ServiceCallbacksID callback_id,
    const std::string* error) {
  ServiceCallbacks* service_callbacks = callbacks_map_.Lookup(callback_id);
  DCHECK(service_callbacks);
  if (!error->empty()) {
    DCHECK(!service_callbacks->failure_callback.is_null());
    service_callbacks->failure_callback.Run(*error);
  } else {
    DCHECK(!service_callbacks->set_properties_callback.is_null());
    service_callbacks->set_properties_callback.Run();
  }
  RemoveServiceCallbacks(callback_id);
}

void NetworkingPrivateServiceClient::AfterCreateNetwork(
    ServiceCallbacksID callback_id,
    const std::string* network_guid,
    const std::string* error) {
  ServiceCallbacks* service_callbacks = callbacks_map_.Lookup(callback_id);
  DCHECK(service_callbacks);
  if (!error->empty()) {
    DCHECK(!service_callbacks->failure_callback.is_null());
    service_callbacks->failure_callback.Run(*error);
  } else {
    DCHECK(!service_callbacks->create_network_callback.is_null());
    service_callbacks->create_network_callback.Run(*network_guid);
  }
  RemoveServiceCallbacks(callback_id);
}

void NetworkingPrivateServiceClient::AfterStartConnect(
    ServiceCallbacksID callback_id,
    const std::string* error) {
  ServiceCallbacks* service_callbacks = callbacks_map_.Lookup(callback_id);
  DCHECK(service_callbacks);
  if (!error->empty()) {
    DCHECK(!service_callbacks->failure_callback.is_null());
    service_callbacks->failure_callback.Run(*error);
  } else {
    DCHECK(!service_callbacks->start_connect_callback.is_null());
    service_callbacks->start_connect_callback.Run();
  }
  RemoveServiceCallbacks(callback_id);
}

void NetworkingPrivateServiceClient::AfterStartDisconnect(
    ServiceCallbacksID callback_id,
    const std::string* error) {
  ServiceCallbacks* service_callbacks = callbacks_map_.Lookup(callback_id);
  DCHECK(service_callbacks);
  if (!error->empty()) {
    DCHECK(!service_callbacks->failure_callback.is_null());
    service_callbacks->failure_callback.Run(*error);
  } else {
    DCHECK(!service_callbacks->start_disconnect_callback.is_null());
    service_callbacks->start_disconnect_callback.Run();
  }
  RemoveServiceCallbacks(callback_id);
}

void NetworkingPrivateServiceClient::AfterVerifyDestination(
    ServiceCallbacksID callback_id,
    const bool* result,
    const std::string* error) {
  ServiceCallbacks* service_callbacks = callbacks_map_.Lookup(callback_id);
  DCHECK(service_callbacks);
  if (!error->empty()) {
    DCHECK(!service_callbacks->failure_callback.is_null());
    service_callbacks->failure_callback.Run(*error);
  } else {
    DCHECK(!service_callbacks->verify_destination_callback.is_null());
    service_callbacks->verify_destination_callback.Run(*result);
  }
  RemoveServiceCallbacks(callback_id);
}

void NetworkingPrivateServiceClient::AfterVerifyAndEncryptCredentials(
    ServiceCallbacksID callback_id,
    const std::string& encrypted_data,
    const std::string& error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ServiceCallbacks* service_callbacks = callbacks_map_.Lookup(callback_id);
  DCHECK(service_callbacks);
  if (!error.empty()) {
    DCHECK(!service_callbacks->failure_callback.is_null());
    service_callbacks->failure_callback.Run(error);
  } else {
    DCHECK(
        !service_callbacks->verify_and_encrypt_credentials_callback.is_null());
    service_callbacks->verify_and_encrypt_credentials_callback.Run(
        encrypted_data);
  }
  RemoveServiceCallbacks(callback_id);
}

void NetworkingPrivateServiceClient::AfterVerifyAndEncryptData(
    ServiceCallbacksID callback_id,
    const std::string* result,
    const std::string* error) {
  ServiceCallbacks* service_callbacks = callbacks_map_.Lookup(callback_id);
  DCHECK(service_callbacks);
  if (!error->empty()) {
    DCHECK(!service_callbacks->failure_callback.is_null());
    service_callbacks->failure_callback.Run(*error);
  } else {
    DCHECK(!service_callbacks->verify_and_encrypt_data_callback.is_null());
    service_callbacks->verify_and_encrypt_data_callback.Run(*result);
  }
  RemoveServiceCallbacks(callback_id);
}

void NetworkingPrivateServiceClient::OnNetworksChangedEventOnUIThread(
    const std::vector<std::string>& network_guids) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  FOR_EACH_OBSERVER(Observer,
                    network_events_observers_,
                    OnNetworksChangedEvent(network_guids));
}

void NetworkingPrivateServiceClient::OnNetworkListChangedEventOnUIThread(
    const std::vector<std::string>& network_guids) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  FOR_EACH_OBSERVER(Observer,
                    network_events_observers_,
                    OnNetworkListChangedEvent(network_guids));
}

}  // namespace extensions
