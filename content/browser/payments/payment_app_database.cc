// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/payment_app_database.h"

#include <utility>

#include "base/bind.h"
#include "base/optional.h"
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

PaymentInstrumentPtr DeserializePaymentInstrument(const std::string& input) {
  PaymentInstrumentProto instrument_proto;
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
    const WriteManifestCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->FindReadyRegistrationForPattern(
      scope, base::Bind(&PaymentAppDatabase::DidFindRegistrationToWriteManifest,
                        weak_ptr_factory_.GetWeakPtr(),
                        base::Passed(std::move(manifest)), callback));
}

void PaymentAppDatabase::ReadManifest(const GURL& scope,
                                      const ReadManifestCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->FindReadyRegistrationForPattern(
      scope, base::Bind(&PaymentAppDatabase::DidFindRegistrationToReadManifest,
                        weak_ptr_factory_.GetWeakPtr(), callback));
}

void PaymentAppDatabase::ReadAllManifests(
    const ReadAllManifestsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->GetUserDataForAllRegistrations(
      kPaymentAppManifestDataKey,
      base::Bind(&PaymentAppDatabase::DidReadAllManifests,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void PaymentAppDatabase::ReadPaymentInstrument(
    const GURL& scope,
    const std::string& instrumentKey,
    ReadPaymentInstrumentCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->FindReadyRegistrationForPattern(
      scope,
      base::Bind(
          &PaymentAppDatabase::DidFindRegistrationToReadPaymentInstrument,
          weak_ptr_factory_.GetWeakPtr(), instrumentKey,
          base::Passed(std::move(callback))));
}

void PaymentAppDatabase::WritePaymentInstrument(
    const GURL& scope,
    const std::string& instrumentKey,
    PaymentInstrumentPtr instrument,
    WritePaymentInstrumentCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->FindReadyRegistrationForPattern(
      scope,
      base::Bind(
          &PaymentAppDatabase::DidFindRegistrationToWritePaymentInstrument,
          weak_ptr_factory_.GetWeakPtr(), instrumentKey,
          base::Passed(std::move(instrument)),
          base::Passed(std::move(callback))));
}

void PaymentAppDatabase::DidFindRegistrationToWriteManifest(
    payments::mojom::PaymentAppManifestPtr manifest,
    const WriteManifestCallback& callback,
    ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status != SERVICE_WORKER_OK) {
    callback.Run(payments::mojom::PaymentAppManifestError::NO_ACTIVE_WORKER);
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
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void PaymentAppDatabase::DidWriteManifest(const WriteManifestCallback& callback,
                                          ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return callback.Run(status == SERVICE_WORKER_OK
                          ? payments::mojom::PaymentAppManifestError::NONE
                          : payments::mojom::PaymentAppManifestError::
                                MANIFEST_STORAGE_OPERATION_FAILED);
}

void PaymentAppDatabase::DidFindRegistrationToReadManifest(
    const ReadManifestCallback& callback,
    ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status != SERVICE_WORKER_OK) {
    callback.Run(payments::mojom::PaymentAppManifest::New(),
                 payments::mojom::PaymentAppManifestError::NO_ACTIVE_WORKER);
    return;
  }

  service_worker_context_->GetRegistrationUserData(
      registration->id(), {kPaymentAppManifestDataKey},
      base::Bind(&PaymentAppDatabase::DidReadManifest,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void PaymentAppDatabase::DidReadManifest(const ReadManifestCallback& callback,
                                         const std::vector<std::string>& data,
                                         ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status != SERVICE_WORKER_OK || data.size() != 1) {
    callback.Run(payments::mojom::PaymentAppManifest::New(),
                 payments::mojom::PaymentAppManifestError::
                     MANIFEST_STORAGE_OPERATION_FAILED);
    return;
  }

  payments::mojom::PaymentAppManifestPtr manifest =
      DeserializePaymentAppManifest(data[0]);
  if (!manifest) {
    callback.Run(payments::mojom::PaymentAppManifest::New(),
                 payments::mojom::PaymentAppManifestError::
                     MANIFEST_STORAGE_OPERATION_FAILED);
    return;
  }

  callback.Run(std::move(manifest),
               payments::mojom::PaymentAppManifestError::NONE);
}

void PaymentAppDatabase::DidReadAllManifests(
    const ReadAllManifestsCallback& callback,
    const std::vector<std::pair<int64_t, std::string>>& raw_data,
    ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status != SERVICE_WORKER_OK) {
    callback.Run(Manifests());
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

  callback.Run(std::move(manifests));
}

void PaymentAppDatabase::DidFindRegistrationToReadPaymentInstrument(
    const std::string& instrumentKey,
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
      registration->id(), {instrumentKey},
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

  PaymentInstrumentPtr instrument = DeserializePaymentInstrument(data[0]);
  if (!instrument) {
    std::move(callback).Run(PaymentInstrument::New(),
                            PaymentHandlerStatus::STORAGE_OPERATION_FAILED);
    return;
  }

  std::move(callback).Run(std::move(instrument), PaymentHandlerStatus::SUCCESS);
}

void PaymentAppDatabase::DidFindRegistrationToWritePaymentInstrument(
    const std::string& instrumentKey,
    PaymentInstrumentPtr instrument,
    WritePaymentInstrumentCallback callback,
    ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status != SERVICE_WORKER_OK) {
    std::move(callback).Run(PaymentHandlerStatus::NO_ACTIVE_WORKER);
    return;
  }

  PaymentInstrumentProto instrument_proto;
  instrument_proto.set_name(instrument->name);
  for (const auto& method : instrument->enabled_methods) {
    instrument_proto.add_enabled_methods(method);
  }
  instrument_proto.set_stringified_capabilities(
      instrument->stringified_capabilities);

  std::string serialized;
  bool success = instrument_proto.SerializeToString(&serialized);
  DCHECK(success);

  service_worker_context_->StoreRegistrationUserData(
      registration->id(), registration->pattern().GetOrigin(),
      {{instrumentKey, serialized}},
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

}  // namespace content
