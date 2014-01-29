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
#include "chrome/browser/extensions/api/networking_private/networking_private_crypto.h"
#include "chrome/common/extensions/api/networking_private.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_host.h"

using content::BrowserThread;
using extensions::api::networking_private::VerificationProperties;

namespace extensions {

namespace {

const char kNetworkingPrivateSequenceTokenName[] = "NetworkingPrivate";

// Implementation of Verify* methods using NetworkingPrivateCrypto.
// TODO(mef): Move this into NetworkingPrivateCrypto class.
class CryptoVerifyImpl : public NetworkingPrivateServiceClient::CryptoVerify {
  bool VerifyDestination(const VerificationProperties& properties) {
    std::vector<std::string> data_parts;
    data_parts.push_back(properties.device_ssid);
    data_parts.push_back(properties.device_serial);
    data_parts.push_back(properties.device_bssid);
    data_parts.push_back(properties.public_key);
    data_parts.push_back(properties.nonce);
    std::string unsigned_data = JoinString(data_parts, ",");
    std::string signed_data;
    if (!base::Base64Decode(properties.signed_data, &signed_data))
      return false;
    NetworkingPrivateCrypto crypto;
    return crypto.VerifyCredentials(properties.certificate,
                                    signed_data,
                                    unsigned_data,
                                    properties.device_bssid);
  }

  virtual void VerifyDestination(scoped_ptr<base::ListValue> args,
                                 bool* verified,
                                 std::string* error) OVERRIDE {
    using extensions::api::networking_private::VerifyDestination::Params;
    scoped_ptr<Params> params = Params::Create(*args);
    *verified = VerifyDestination(params->properties);
  }

  virtual void VerifyAndEncryptData(scoped_ptr<base::ListValue> args,
                                    std::string* base64_encoded_ciphertext,
                                    std::string* error) OVERRIDE {
    using extensions::api::networking_private::VerifyAndEncryptData::Params;
    scoped_ptr<Params> params = Params::Create(*args);

    if (!VerifyDestination(params->properties)) {
      *error = "VerifyError";
      return;
    }

    std::string public_key;
    if (!base::Base64Decode(params->properties.public_key, &public_key)) {
      *error = "DecodeError";
      return;
    }

    NetworkingPrivateCrypto crypto;
    std::string ciphertext;
    if (!crypto.EncryptByteString(public_key, params->data, &ciphertext)) {
      *error = "EncryptError";
      return;
    }

    base::Base64Encode(ciphertext, base64_encoded_ciphertext);
  }
};

// Deletes WiFiService and CryptoVerify objects on worker thread.
void ShutdownServicesOnWorkerThread(
    scoped_ptr<wifi::WiFiService> wifi_service,
    scoped_ptr<NetworkingPrivateServiceClient::CryptoVerify> crypto_verify) {
  DCHECK(wifi_service.get());
  DCHECK(crypto_verify.get());
}

}  // namespace

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
  task_runner_->PostTask(
    FROM_HERE,
    base::Bind(
        &WiFiService::Initialize,
        base::Unretained(wifi_service_.get()),
        task_runner_));
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
}

NetworkingPrivateServiceClient::~NetworkingPrivateServiceClient() {
  // Verify that these objects were passed to ShutdownServicesOnWorkerThread to
  // be deleted after completion of all posted tasks.
  DCHECK(!wifi_service_.get());
  DCHECK(!crypto_verify_.get());
}

NetworkingPrivateServiceClient::CryptoVerify*
    NetworkingPrivateServiceClient::CryptoVerify::Create() {
  return new CryptoVerifyImpl();
}

NetworkingPrivateServiceClient::ServiceCallbacks::ServiceCallbacks() {}

NetworkingPrivateServiceClient::ServiceCallbacks::~ServiceCallbacks() {}

void NetworkingPrivateServiceClient::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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

void NetworkingPrivateServiceClient::GetProperties(
    const std::string& network_guid,
    const DictionaryResultCallback& callback,
    const ErrorCallback& error_callback) {
  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->error_callback = error_callback;
  service_callbacks->get_properties_callback = callback;

  base::DictionaryValue* properties = new base::DictionaryValue();
  std::string* error = new std::string;

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&WiFiService::GetProperties,
                 base::Unretained(wifi_service_.get()),
                 network_guid,
                 properties,
                 error),
      base::Bind(&NetworkingPrivateServiceClient::AfterGetProperties,
                 weak_factory_.GetWeakPtr(),
                 service_callbacks->id,
                 network_guid,
                 base::Owned(properties),
                 base::Owned(error)));
}

void NetworkingPrivateServiceClient::GetManagedProperties(
    const std::string& network_guid,
    const DictionaryResultCallback& callback,
    const ErrorCallback& error_callback) {
  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->error_callback = error_callback;
  service_callbacks->get_properties_callback = callback;

  base::DictionaryValue* properties = new base::DictionaryValue();
  std::string* error = new std::string;

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&WiFiService::GetManagedProperties,
                 base::Unretained(wifi_service_.get()),
                 network_guid,
                 properties,
                 error),
      base::Bind(&NetworkingPrivateServiceClient::AfterGetProperties,
                 weak_factory_.GetWeakPtr(),
                 service_callbacks->id,
                 network_guid,
                 base::Owned(properties),
                 base::Owned(error)));
}

void NetworkingPrivateServiceClient::GetState(
    const std::string& network_guid,
    const DictionaryResultCallback& callback,
    const ErrorCallback& error_callback) {
  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->error_callback = error_callback;
  service_callbacks->get_properties_callback = callback;

  base::DictionaryValue* properties = new base::DictionaryValue();
  std::string* error = new std::string;

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&WiFiService::GetState,
                 base::Unretained(wifi_service_.get()),
                 network_guid,
                 properties,
                 error),
      base::Bind(&NetworkingPrivateServiceClient::AfterGetProperties,
                 weak_factory_.GetWeakPtr(),
                 service_callbacks->id,
                 network_guid,
                 base::Owned(properties),
                 base::Owned(error)));
}

void NetworkingPrivateServiceClient::GetVisibleNetworks(
    const std::string& network_type,
    const ListResultCallback& callback) {
  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->get_visible_networks_callback = callback;

  base::ListValue* networks = new base::ListValue();

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&WiFiService::GetVisibleNetworks,
                 base::Unretained(wifi_service_.get()),
                 network_type,
                 networks),
      base::Bind(&NetworkingPrivateServiceClient::AfterGetVisibleNetworks,
                 weak_factory_.GetWeakPtr(),
                 service_callbacks->id,
                 base::Owned(networks)));
}

void NetworkingPrivateServiceClient::RequestNetworkScan() {
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&WiFiService::RequestNetworkScan,
                 base::Unretained(wifi_service_.get())));
}

void NetworkingPrivateServiceClient::SetProperties(
    const std::string& network_guid,
    const base::DictionaryValue& properties,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->error_callback = error_callback;
  service_callbacks->set_properties_callback = callback;

  scoped_ptr<base::DictionaryValue> properties_ptr(properties.DeepCopy());
  std::string* error = new std::string;

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&WiFiService::SetProperties,
                 base::Unretained(wifi_service_.get()),
                 network_guid,
                 base::Passed(&properties_ptr),
                 error),
      base::Bind(&NetworkingPrivateServiceClient::AfterSetProperties,
                 weak_factory_.GetWeakPtr(),
                 service_callbacks->id,
                 base::Owned(error)));
}

void NetworkingPrivateServiceClient::CreateNetwork(
    bool shared,
    const base::DictionaryValue& properties,
    const StringResultCallback& callback,
    const ErrorCallback& error_callback) {
  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->error_callback = error_callback;
  service_callbacks->create_network_callback = callback;

  scoped_ptr<base::DictionaryValue> properties_ptr(properties.DeepCopy());
  std::string* network_guid = new std::string;
  std::string* error = new std::string;

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&WiFiService::CreateNetwork,
                 base::Unretained(wifi_service_.get()),
                 shared,
                 base::Passed(&properties_ptr),
                 network_guid,
                 error),
      base::Bind(&NetworkingPrivateServiceClient::AfterCreateNetwork,
                 weak_factory_.GetWeakPtr(),
                 service_callbacks->id,
                 base::Owned(network_guid),
                 base::Owned(error)));
}

void NetworkingPrivateServiceClient::StartConnect(
    const std::string& network_guid,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->error_callback = error_callback;
  service_callbacks->start_connect_callback = callback;

  std::string* error = new std::string;

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&WiFiService::StartConnect,
                 base::Unretained(wifi_service_.get()),
                 network_guid,
                 error),
      base::Bind(&NetworkingPrivateServiceClient::AfterStartConnect,
                 weak_factory_.GetWeakPtr(),
                 service_callbacks->id,
                 base::Owned(error)));
}

void NetworkingPrivateServiceClient::StartDisconnect(
    const std::string& network_guid,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->error_callback = error_callback;
  service_callbacks->start_disconnect_callback = callback;

  std::string* error = new std::string;

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&WiFiService::StartDisconnect,
                 base::Unretained(wifi_service_.get()),
                 network_guid,
                 error),
      base::Bind(&NetworkingPrivateServiceClient::AfterStartDisconnect,
                 weak_factory_.GetWeakPtr(),
                 service_callbacks->id,
                 base::Owned(error)));
}

void NetworkingPrivateServiceClient::VerifyDestination(
    scoped_ptr<base::ListValue> args,
    const BoolResultCallback& callback,
    const CryptoErrorCallback& error_callback) {
  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->crypto_error_callback = error_callback;
  service_callbacks->verify_destination_callback = callback;

  bool* result = new bool;
  std::string* error = new std::string;

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&CryptoVerify::VerifyDestination,
                 base::Unretained(crypto_verify_.get()),
                 base::Passed(&args),
                 result,
                 error),
      base::Bind(&NetworkingPrivateServiceClient::AfterVerifyDestination,
                 weak_factory_.GetWeakPtr(),
                 service_callbacks->id,
                 base::Owned(result),
                 base::Owned(error)));
}

void NetworkingPrivateServiceClient::VerifyAndEncryptData(
    scoped_ptr<base::ListValue> args,
    const StringResultCallback& callback,
    const CryptoErrorCallback& error_callback) {
  ServiceCallbacks* service_callbacks = AddServiceCallbacks();
  service_callbacks->crypto_error_callback = error_callback;
  service_callbacks->verify_and_encrypt_data_callback = callback;

  std::string* result = new std::string;
  std::string* error = new std::string;

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&CryptoVerify::VerifyAndEncryptData,
                 base::Unretained(crypto_verify_.get()),
                 base::Passed(&args),
                 result,
                 error),
      base::Bind(&NetworkingPrivateServiceClient::AfterVerifyAndEncryptData,
                 weak_factory_.GetWeakPtr(),
                 service_callbacks->id,
                 base::Owned(result),
                 base::Owned(error)));
}

void NetworkingPrivateServiceClient::AfterGetProperties(
    ServiceCallbacksID callback_id,
    const std::string& network_guid,
    const base::DictionaryValue* properties,
    const std::string* error) {
  ServiceCallbacks* service_callbacks = callbacks_map_.Lookup(callback_id);
  DCHECK(service_callbacks);
  if (!error->empty()) {
    DCHECK(!service_callbacks->error_callback.is_null());
    service_callbacks->error_callback.Run(*error,
                                          scoped_ptr<base::DictionaryValue>());
  } else {
    DCHECK(!service_callbacks->get_properties_callback.is_null());
    service_callbacks->get_properties_callback.Run(network_guid, *properties);
  }
  RemoveServiceCallbacks(callback_id);
}

void NetworkingPrivateServiceClient::AfterGetVisibleNetworks(
    ServiceCallbacksID callback_id,
    const base::ListValue* networks) {
  ServiceCallbacks* service_callbacks = callbacks_map_.Lookup(callback_id);
  DCHECK(service_callbacks);
  DCHECK(!service_callbacks->get_visible_networks_callback.is_null());
  service_callbacks->get_visible_networks_callback.Run(*networks);
  RemoveServiceCallbacks(callback_id);
}

void NetworkingPrivateServiceClient::AfterSetProperties(
    ServiceCallbacksID callback_id,
    const std::string* error) {
  ServiceCallbacks* service_callbacks = callbacks_map_.Lookup(callback_id);
  DCHECK(service_callbacks);
  if (!error->empty()) {
    DCHECK(!service_callbacks->error_callback.is_null());
    service_callbacks->error_callback.Run(*error,
                                          scoped_ptr<base::DictionaryValue>());
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
    DCHECK(!service_callbacks->error_callback.is_null());
    service_callbacks->error_callback.Run(*error,
                                          scoped_ptr<base::DictionaryValue>());
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
    DCHECK(!service_callbacks->error_callback.is_null());
    service_callbacks->error_callback.Run(*error,
                                          scoped_ptr<base::DictionaryValue>());
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
    DCHECK(!service_callbacks->error_callback.is_null());
    service_callbacks->error_callback.Run(*error,
                                          scoped_ptr<base::DictionaryValue>());
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
    DCHECK(!service_callbacks->crypto_error_callback.is_null());
    service_callbacks->crypto_error_callback.Run(*error, *error);
  } else {
    DCHECK(!service_callbacks->verify_destination_callback.is_null());
    service_callbacks->verify_destination_callback.Run(*result);
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
    DCHECK(!service_callbacks->crypto_error_callback.is_null());
    service_callbacks->crypto_error_callback.Run(*error, *error);
  } else {
    DCHECK(!service_callbacks->verify_and_encrypt_data_callback.is_null());
    service_callbacks->verify_and_encrypt_data_callback.Run(*result);
  }
  RemoveServiceCallbacks(callback_id);
}

void NetworkingPrivateServiceClient::OnNetworksChangedEventOnUIThread(
    const std::vector<std::string>& network_guids) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FOR_EACH_OBSERVER(Observer,
                    network_events_observers_,
                    OnNetworksChangedEvent(network_guids));
}

void NetworkingPrivateServiceClient::OnNetworkListChangedEventOnUIThread(
    const std::vector<std::string>& network_guids) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FOR_EACH_OBSERVER(Observer,
                    network_events_observers_,
                    OnNetworkListChangedEvent(network_guids));
}

}  // namespace extensions
