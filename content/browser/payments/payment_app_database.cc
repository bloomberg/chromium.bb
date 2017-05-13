// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/payment_app_database.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "content/browser/payments/payment_app.pb.h"
#include "content/browser/payments/payment_app_context_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/public/browser/browser_thread.h"

namespace content {
namespace {

using ::payments::mojom::PaymentHandlerStatus;
using ::payments::mojom::PaymentInstrument;
using ::payments::mojom::PaymentInstrumentPtr;

const char kPaymentAppManifestDataKey[] = "PaymentAppManifestData";
const char kPaymentInstrumentPrefix[] = "PaymentInstrument:";
const char kPaymentInstrumentKeyInfoPrefix[] = "PaymentInstrumentKeyInfo:";

std::string CreatePaymentInstrumentKey(const std::string& instrument_key) {
  return kPaymentInstrumentPrefix + instrument_key;
}

std::string CreatePaymentInstrumentKeyInfoKey(
    const std::string& instrument_key) {
  return kPaymentInstrumentKeyInfoPrefix + instrument_key;
}

payments::mojom::PaymentAppManifestPtr DeserializePaymentAppManifest(
    const std::string& input) {
  PaymentAppManifestProto manifest_proto;
  if (!manifest_proto.ParseFromString(input))
    return nullptr;

  payments::mojom::PaymentAppManifestPtr manifest =
      payments::mojom::PaymentAppManifest::New();
  manifest->name = manifest_proto.name();
  if (manifest_proto.has_icon())
    manifest->icon = manifest_proto.icon();
  for (const auto& option_proto : manifest_proto.options()) {
    payments::mojom::PaymentAppOptionPtr option =
        payments::mojom::PaymentAppOption::New();
    option->name = option_proto.name();
    if (option_proto.has_icon())
      option->icon = option_proto.icon();
    option->id = option_proto.id();
    for (const auto& method : option_proto.enabled_methods())
      option->enabled_methods.push_back(method);
    manifest->options.push_back(std::move(option));
  }

  return manifest;
}

std::map<uint64_t, std::string> ToStoredPaymentInstrumentKeyInfos(
    const std::vector<std::string>& inputs) {
  std::map<uint64_t, std::string> key_info;
  for (const auto& input : inputs) {
    StoredPaymentInstrumentKeyInfoProto key_info_proto;
    if (!key_info_proto.ParseFromString(input))
      return std::map<uint64_t, std::string>();

    key_info.insert(std::pair<uint64_t, std::string>(
        key_info_proto.insertion_order(), key_info_proto.key()));
  }

  return key_info;
}

PaymentInstrumentPtr ToPaymentInstrumentForMojo(const std::string& input) {
  StoredPaymentInstrumentProto instrument_proto;
  if (!instrument_proto.ParseFromString(input))
    return nullptr;

  PaymentInstrumentPtr instrument = PaymentInstrument::New();
  instrument->name = instrument_proto.name();
  for (const auto& method : instrument_proto.enabled_methods())
    instrument->enabled_methods.push_back(method);
  instrument->stringified_capabilities =
      instrument_proto.stringified_capabilities();

  return instrument;
}

std::unique_ptr<StoredPaymentInstrument> ToStoredPaymentInstrument(
    const std::string& input) {
  StoredPaymentInstrumentProto instrument_proto;
  if (!instrument_proto.ParseFromString(input))
    return std::unique_ptr<StoredPaymentInstrument>();

  std::unique_ptr<StoredPaymentInstrument> instrument =
      base::MakeUnique<StoredPaymentInstrument>();
  instrument->registration_id = instrument_proto.registration_id();
  instrument->instrument_key = instrument_proto.instrument_key();
  instrument->origin = GURL(instrument_proto.origin());
  instrument->name = instrument_proto.name();
  for (const auto& method : instrument_proto.enabled_methods())
    instrument->enabled_methods.push_back(method);

  return instrument;
}

}  // namespace

PaymentAppDatabase::PaymentAppDatabase(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : service_worker_context_(service_worker_context), weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

PaymentAppDatabase::~PaymentAppDatabase() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void PaymentAppDatabase::WriteManifest(
    const GURL& scope,
    payments::mojom::PaymentAppManifestPtr manifest,
    WriteManifestCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->FindReadyRegistrationForPattern(
      scope, base::Bind(&PaymentAppDatabase::DidFindRegistrationToWriteManifest,
                        weak_ptr_factory_.GetWeakPtr(), base::Passed(&manifest),
                        base::Passed(&callback)));
}

void PaymentAppDatabase::ReadManifest(const GURL& scope,
                                      ReadManifestCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->FindReadyRegistrationForPattern(
      scope,
      base::Bind(&PaymentAppDatabase::DidFindRegistrationToReadManifest,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&callback)));
}

void PaymentAppDatabase::ReadAllManifests(ReadAllManifestsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->GetUserDataForAllRegistrations(
      kPaymentAppManifestDataKey,
      base::Bind(&PaymentAppDatabase::DidReadAllManifests,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&callback)));
}

void PaymentAppDatabase::ReadAllPaymentApps(
    ReadAllPaymentAppsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->GetUserDataForAllRegistrationsByKeyPrefix(
      kPaymentInstrumentPrefix,
      base::Bind(&PaymentAppDatabase::DidReadAllPaymentApps,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(std::move(callback))));
}

void PaymentAppDatabase::DeletePaymentInstrument(
    const GURL& scope,
    const std::string& instrument_key,
    DeletePaymentInstrumentCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->FindReadyRegistrationForPattern(
      scope,
      base::Bind(
          &PaymentAppDatabase::DidFindRegistrationToDeletePaymentInstrument,
          weak_ptr_factory_.GetWeakPtr(), instrument_key,
          base::Passed(std::move(callback))));
}

void PaymentAppDatabase::ReadPaymentInstrument(
    const GURL& scope,
    const std::string& instrument_key,
    ReadPaymentInstrumentCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->FindReadyRegistrationForPattern(
      scope,
      base::Bind(
          &PaymentAppDatabase::DidFindRegistrationToReadPaymentInstrument,
          weak_ptr_factory_.GetWeakPtr(), instrument_key,
          base::Passed(std::move(callback))));
}

void PaymentAppDatabase::KeysOfPaymentInstruments(
    const GURL& scope,
    KeysOfPaymentInstrumentsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->FindReadyRegistrationForPattern(
      scope, base::Bind(&PaymentAppDatabase::DidFindRegistrationToGetKeys,
                        weak_ptr_factory_.GetWeakPtr(),
                        base::Passed(std::move(callback))));
}

void PaymentAppDatabase::HasPaymentInstrument(
    const GURL& scope,
    const std::string& instrument_key,
    HasPaymentInstrumentCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->FindReadyRegistrationForPattern(
      scope,
      base::Bind(&PaymentAppDatabase::DidFindRegistrationToHasPaymentInstrument,
                 weak_ptr_factory_.GetWeakPtr(), instrument_key,
                 base::Passed(std::move(callback))));
}

void PaymentAppDatabase::WritePaymentInstrument(
    const GURL& scope,
    const std::string& instrument_key,
    PaymentInstrumentPtr instrument,
    WritePaymentInstrumentCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->FindReadyRegistrationForPattern(
      scope,
      base::Bind(
          &PaymentAppDatabase::DidFindRegistrationToWritePaymentInstrument,
          weak_ptr_factory_.GetWeakPtr(), instrument_key,
          base::Passed(std::move(instrument)),
          base::Passed(std::move(callback))));
}

void PaymentAppDatabase::ClearPaymentInstruments(
    const GURL& scope,
    ClearPaymentInstrumentsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->FindReadyRegistrationForPattern(
      scope,
      base::Bind(
          &PaymentAppDatabase::DidFindRegistrationToClearPaymentInstruments,
          weak_ptr_factory_.GetWeakPtr(), scope,
          base::Passed(std::move(callback))));
}

void PaymentAppDatabase::DidFindRegistrationToWriteManifest(
    payments::mojom::PaymentAppManifestPtr manifest,
    WriteManifestCallback callback,
    ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status != SERVICE_WORKER_OK) {
    std::move(callback).Run(
        payments::mojom::PaymentAppManifestError::NO_ACTIVE_WORKER);
    return;
  }

  PaymentAppManifestProto manifest_proto;
  manifest_proto.set_name(manifest->name);
  if (manifest->icon)
    manifest_proto.set_icon(manifest->icon.value());

  for (const auto& option : manifest->options) {
    PaymentAppOptionProto* option_proto = manifest_proto.add_options();
    option_proto->set_name(option->name);
    if (option->icon)
      option_proto->set_icon(option->icon.value());
    option_proto->set_id(option->id);
    for (const auto& method : option->enabled_methods) {
      option_proto->add_enabled_methods(method);
    }
  }

  std::string serialized;
  bool success = manifest_proto.SerializeToString(&serialized);
  DCHECK(success);

  service_worker_context_->StoreRegistrationUserData(
      registration->id(), registration->pattern().GetOrigin(),
      {{kPaymentAppManifestDataKey, serialized}},
      base::Bind(&PaymentAppDatabase::DidWriteManifest,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&callback)));
}

void PaymentAppDatabase::DidWriteManifest(WriteManifestCallback callback,
                                          ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::move(callback).Run(status == SERVICE_WORKER_OK
                              ? payments::mojom::PaymentAppManifestError::NONE
                              : payments::mojom::PaymentAppManifestError::
                                    MANIFEST_STORAGE_OPERATION_FAILED);
}

void PaymentAppDatabase::DidFindRegistrationToReadManifest(
    ReadManifestCallback callback,
    ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status != SERVICE_WORKER_OK) {
    std::move(callback).Run(
        payments::mojom::PaymentAppManifest::New(),
        payments::mojom::PaymentAppManifestError::NO_ACTIVE_WORKER);
    return;
  }

  service_worker_context_->GetRegistrationUserData(
      registration->id(), {kPaymentAppManifestDataKey},
      base::Bind(&PaymentAppDatabase::DidReadManifest,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&callback)));
}

void PaymentAppDatabase::DidReadManifest(ReadManifestCallback callback,
                                         const std::vector<std::string>& data,
                                         ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status != SERVICE_WORKER_OK || data.size() != 1) {
    std::move(callback).Run(payments::mojom::PaymentAppManifest::New(),
                            payments::mojom::PaymentAppManifestError::
                                MANIFEST_STORAGE_OPERATION_FAILED);
    return;
  }

  payments::mojom::PaymentAppManifestPtr manifest =
      DeserializePaymentAppManifest(data[0]);
  if (!manifest) {
    std::move(callback).Run(payments::mojom::PaymentAppManifest::New(),
                            payments::mojom::PaymentAppManifestError::
                                MANIFEST_STORAGE_OPERATION_FAILED);
    return;
  }

  std::move(callback).Run(std::move(manifest),
                          payments::mojom::PaymentAppManifestError::NONE);
}

void PaymentAppDatabase::DidReadAllManifests(
    ReadAllManifestsCallback callback,
    const std::vector<std::pair<int64_t, std::string>>& raw_data,
    ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status != SERVICE_WORKER_OK) {
    std::move(callback).Run(Manifests());
    return;
  }

  Manifests manifests;
  for (const auto& item_of_raw_data : raw_data) {
    payments::mojom::PaymentAppManifestPtr manifest =
        DeserializePaymentAppManifest(item_of_raw_data.second);
    if (!manifest)
      continue;

    manifests.push_back(
        ManifestWithID(item_of_raw_data.first, std::move(manifest)));
  }

  std::move(callback).Run(std::move(manifests));
}

void PaymentAppDatabase::DidReadAllPaymentApps(
    ReadAllPaymentAppsCallback callback,
    const std::vector<std::pair<int64_t, std::string>>& raw_data,
    ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status != SERVICE_WORKER_OK) {
    std::move(callback).Run(PaymentApps());
    return;
  }

  PaymentApps apps;
  for (const auto& item_of_raw_data : raw_data) {
    std::unique_ptr<StoredPaymentInstrument> instrument =
        ToStoredPaymentInstrument(item_of_raw_data.second);
    if (!instrument)
      continue;
    if (!base::ContainsKey(apps, instrument->origin))
      apps.insert(std::make_pair(instrument->origin, Instruments()));
    apps[instrument->origin].push_back(std::move(instrument));
  }

  std::move(callback).Run(std::move(apps));
}

void PaymentAppDatabase::DidFindRegistrationToDeletePaymentInstrument(
    const std::string& instrument_key,
    DeletePaymentInstrumentCallback callback,
    ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status != SERVICE_WORKER_OK) {
    std::move(callback).Run(PaymentHandlerStatus::NO_ACTIVE_WORKER);
    return;
  }

  service_worker_context_->GetRegistrationUserData(
      registration->id(), {CreatePaymentInstrumentKey(instrument_key)},
      base::Bind(&PaymentAppDatabase::DidFindPaymentInstrument,
                 weak_ptr_factory_.GetWeakPtr(), registration->id(),
                 instrument_key, base::Passed(std::move(callback))));
}

void PaymentAppDatabase::DidFindPaymentInstrument(
    int64_t registration_id,
    const std::string& instrument_key,
    DeletePaymentInstrumentCallback callback,
    const std::vector<std::string>& data,
    ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status != SERVICE_WORKER_OK || data.size() != 1) {
    std::move(callback).Run(PaymentHandlerStatus::NOT_FOUND);
    return;
  }

  service_worker_context_->ClearRegistrationUserData(
      registration_id,
      {CreatePaymentInstrumentKey(instrument_key),
       CreatePaymentInstrumentKeyInfoKey(instrument_key)},
      base::Bind(&PaymentAppDatabase::DidDeletePaymentInstrument,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(std::move(callback))));
}

void PaymentAppDatabase::DidDeletePaymentInstrument(
    DeletePaymentInstrumentCallback callback,
    ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return std::move(callback).Run(status == SERVICE_WORKER_OK
                                     ? PaymentHandlerStatus::SUCCESS
                                     : PaymentHandlerStatus::NOT_FOUND);
}

void PaymentAppDatabase::DidFindRegistrationToReadPaymentInstrument(
    const std::string& instrument_key,
    ReadPaymentInstrumentCallback callback,
    ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status != SERVICE_WORKER_OK) {
    std::move(callback).Run(PaymentInstrument::New(),
                            PaymentHandlerStatus::NO_ACTIVE_WORKER);
    return;
  }

  service_worker_context_->GetRegistrationUserData(
      registration->id(), {CreatePaymentInstrumentKey(instrument_key)},
      base::Bind(&PaymentAppDatabase::DidReadPaymentInstrument,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(std::move(callback))));
}

void PaymentAppDatabase::DidReadPaymentInstrument(
    ReadPaymentInstrumentCallback callback,
    const std::vector<std::string>& data,
    ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status != SERVICE_WORKER_OK || data.size() != 1) {
    std::move(callback).Run(PaymentInstrument::New(),
                            PaymentHandlerStatus::NOT_FOUND);
    return;
  }

  PaymentInstrumentPtr instrument = ToPaymentInstrumentForMojo(data[0]);
  if (!instrument) {
    std::move(callback).Run(PaymentInstrument::New(),
                            PaymentHandlerStatus::STORAGE_OPERATION_FAILED);
    return;
  }

  std::move(callback).Run(std::move(instrument), PaymentHandlerStatus::SUCCESS);
}

void PaymentAppDatabase::DidFindRegistrationToGetKeys(
    KeysOfPaymentInstrumentsCallback callback,
    ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status != SERVICE_WORKER_OK) {
    std::move(callback).Run(std::vector<std::string>(),
                            PaymentHandlerStatus::NO_ACTIVE_WORKER);
    return;
  }

  service_worker_context_->GetRegistrationUserDataByKeyPrefix(
      registration->id(), {kPaymentInstrumentKeyInfoPrefix},
      base::Bind(&PaymentAppDatabase::DidGetKeysOfPaymentInstruments,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(std::move(callback))));
}

void PaymentAppDatabase::DidGetKeysOfPaymentInstruments(
    KeysOfPaymentInstrumentsCallback callback,
    const std::vector<std::string>& data,
    ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status != SERVICE_WORKER_OK) {
    std::move(callback).Run(std::vector<std::string>(),
                            PaymentHandlerStatus::NOT_FOUND);
    return;
  }

  std::vector<std::string> keys;
  for (const auto& key_info : ToStoredPaymentInstrumentKeyInfos(data)) {
    keys.push_back(key_info.second);
  }

  std::move(callback).Run(keys, PaymentHandlerStatus::SUCCESS);
}

void PaymentAppDatabase::DidFindRegistrationToHasPaymentInstrument(
    const std::string& instrument_key,
    HasPaymentInstrumentCallback callback,
    ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status != SERVICE_WORKER_OK) {
    std::move(callback).Run(PaymentHandlerStatus::NO_ACTIVE_WORKER);
    return;
  }

  service_worker_context_->GetRegistrationUserData(
      registration->id(), {CreatePaymentInstrumentKey(instrument_key)},
      base::Bind(&PaymentAppDatabase::DidHasPaymentInstrument,
                 weak_ptr_factory_.GetWeakPtr(), registration->id(),
                 instrument_key, base::Passed(std::move(callback))));
}

void PaymentAppDatabase::DidHasPaymentInstrument(
    int64_t registration_id,
    const std::string& instrument_key,
    DeletePaymentInstrumentCallback callback,
    const std::vector<std::string>& data,
    ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status != SERVICE_WORKER_OK || data.size() != 1) {
    std::move(callback).Run(PaymentHandlerStatus::NOT_FOUND);
    return;
  }

  std::move(callback).Run(PaymentHandlerStatus::SUCCESS);
}

void PaymentAppDatabase::DidFindRegistrationToWritePaymentInstrument(
    const std::string& instrument_key,
    PaymentInstrumentPtr instrument,
    WritePaymentInstrumentCallback callback,
    ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status != SERVICE_WORKER_OK) {
    std::move(callback).Run(PaymentHandlerStatus::NO_ACTIVE_WORKER);
    return;
  }

  StoredPaymentInstrumentProto instrument_proto;
  instrument_proto.set_registration_id(registration->id());
  instrument_proto.set_instrument_key(instrument_key);
  instrument_proto.set_origin(registration->pattern().GetOrigin().spec());
  instrument_proto.set_name(instrument->name);
  for (const auto& method : instrument->enabled_methods) {
    instrument_proto.add_enabled_methods(method);
  }
  instrument_proto.set_stringified_capabilities(
      instrument->stringified_capabilities);

  std::string serialized_instrument;
  bool success = instrument_proto.SerializeToString(&serialized_instrument);
  DCHECK(success);

  StoredPaymentInstrumentKeyInfoProto key_info_proto;
  key_info_proto.set_key(instrument_key);
  key_info_proto.set_insertion_order(base::Time::Now().ToInternalValue());

  std::string serialized_key_info;
  success = key_info_proto.SerializeToString(&serialized_key_info);
  DCHECK(success);

  service_worker_context_->StoreRegistrationUserData(
      registration->id(), registration->pattern().GetOrigin(),
      {{CreatePaymentInstrumentKey(instrument_key), serialized_instrument},
       {CreatePaymentInstrumentKeyInfoKey(instrument_key),
        serialized_key_info}},
      base::Bind(&PaymentAppDatabase::DidWritePaymentInstrument,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(std::move(callback))));
}

void PaymentAppDatabase::DidWritePaymentInstrument(
    WritePaymentInstrumentCallback callback,
    ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return std::move(callback).Run(
      status == SERVICE_WORKER_OK
          ? PaymentHandlerStatus::SUCCESS
          : PaymentHandlerStatus::STORAGE_OPERATION_FAILED);
}

void PaymentAppDatabase::DidFindRegistrationToClearPaymentInstruments(
    const GURL& scope,
    ClearPaymentInstrumentsCallback callback,
    ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (status != SERVICE_WORKER_OK) {
    std::move(callback).Run(PaymentHandlerStatus::NO_ACTIVE_WORKER);
    return;
  }

  KeysOfPaymentInstruments(
      scope,
      base::BindOnce(&PaymentAppDatabase::DidGetKeysToClearPaymentInstruments,
                     weak_ptr_factory_.GetWeakPtr(), registration->id(),
                     std::move(callback)));
}

void PaymentAppDatabase::DidGetKeysToClearPaymentInstruments(
    int64_t registration_id,
    ClearPaymentInstrumentsCallback callback,
    const std::vector<std::string>& keys,
    PaymentHandlerStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (status != PaymentHandlerStatus::SUCCESS) {
    std::move(callback).Run(PaymentHandlerStatus::NOT_FOUND);
    return;
  }

  std::vector<std::string> keys_with_prefix;
  for (const auto& key : keys) {
    keys_with_prefix.push_back(CreatePaymentInstrumentKey(key));
    keys_with_prefix.push_back(CreatePaymentInstrumentKeyInfoKey(key));
  }

  service_worker_context_->ClearRegistrationUserData(
      registration_id, keys_with_prefix,
      base::Bind(&PaymentAppDatabase::DidClearPaymentInstruments,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(std::move(callback))));
}

void PaymentAppDatabase::DidClearPaymentInstruments(
    ClearPaymentInstrumentsCallback callback,
    ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return std::move(callback).Run(status == SERVICE_WORKER_OK
                                     ? PaymentHandlerStatus::SUCCESS
                                     : PaymentHandlerStatus::NOT_FOUND);
}

}  // namespace content
