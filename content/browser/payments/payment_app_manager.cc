// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/payment_app_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/optional.h"
#include "content/browser/payments/payment_app.pb.h"
#include "content/browser/payments/payment_app_context.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/public/browser/browser_thread.h"

namespace content {
namespace {

const char kPaymentAppManifestDataKey[] = "PaymentAppManifestData";

}  // namespace

PaymentAppManager::~PaymentAppManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

PaymentAppManager::PaymentAppManager(
    PaymentAppContext* payment_app_context,
    mojo::InterfaceRequest<payments::mojom::PaymentAppManager> request)
    : payment_app_context_(payment_app_context),
      binding_(this, std::move(request)),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(payment_app_context);

  binding_.set_connection_error_handler(
      base::Bind(&PaymentAppManager::OnConnectionError,
                 base::Unretained(this)));
}

void PaymentAppManager::OnConnectionError() {
  payment_app_context_->ServiceHadConnectionError(this);
}

void PaymentAppManager::SetManifest(
    const std::string& scope,
    payments::mojom::PaymentAppManifestPtr manifest,
    const SetManifestCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(zino): Should implement requesting a permission for users to allow
  // the payment app to be registered. Please see http://crbug.com/665949.

  payment_app_context_->service_worker_context()
      ->FindReadyRegistrationForPattern(
          GURL(scope),
          base::Bind(&PaymentAppManager::DidFindRegistrationToSetManifest,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(std::move(manifest)), callback));
}

void PaymentAppManager::GetManifest(const std::string& scope,
                                    const GetManifestCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  payment_app_context_->service_worker_context()
      ->FindReadyRegistrationForPattern(
          GURL(scope),
          base::Bind(&PaymentAppManager::DidFindRegistrationToGetManifest,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void PaymentAppManager::DidFindRegistrationToSetManifest(
    payments::mojom::PaymentAppManifestPtr manifest,
    const SetManifestCallback& callback,
    ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status != SERVICE_WORKER_OK) {
    callback.Run(payments::mojom::PaymentAppManifestError::NO_ACTIVE_WORKER);
    return;
  }

  PaymentAppManifestProto manifest_proto;
  manifest_proto.set_label(manifest->label);
  if (manifest->icon)
    manifest_proto.set_icon(manifest->icon.value());

  for (const auto& option : manifest->options) {
    PaymentAppOptionProto* option_proto = manifest_proto.add_options();
    option_proto->set_label(option->label);
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

  payment_app_context_->service_worker_context()->StoreRegistrationUserData(
      registration->id(), registration->pattern().GetOrigin(),
      {{kPaymentAppManifestDataKey, serialized}},
      base::Bind(&PaymentAppManager::DidSetManifest,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void PaymentAppManager::DidSetManifest(const SetManifestCallback& callback,
                                       ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return callback.Run(status == SERVICE_WORKER_OK
                          ? payments::mojom::PaymentAppManifestError::NONE
                          : payments::mojom::PaymentAppManifestError::
                                MANIFEST_STORAGE_OPERATION_FAILED);
}

void PaymentAppManager::DidFindRegistrationToGetManifest(
    const GetManifestCallback& callback,
    ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status != SERVICE_WORKER_OK) {
    callback.Run(payments::mojom::PaymentAppManifest::New(),
                 payments::mojom::PaymentAppManifestError::NO_ACTIVE_WORKER);
    return;
  }

  payment_app_context_->service_worker_context()->GetRegistrationUserData(
      registration->id(), {kPaymentAppManifestDataKey},
      base::Bind(&PaymentAppManager::DidGetManifest,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void PaymentAppManager::DidGetManifest(const GetManifestCallback& callback,
                                       const std::vector<std::string>& data,
                                       ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status != SERVICE_WORKER_OK || data.size() != 1) {
    callback.Run(payments::mojom::PaymentAppManifest::New(),
                 payments::mojom::PaymentAppManifestError::
                     MANIFEST_STORAGE_OPERATION_FAILED);
    return;
  }

  PaymentAppManifestProto manifest_proto;
  bool success = manifest_proto.ParseFromString(data[0]);
  if (!success) {
    callback.Run(payments::mojom::PaymentAppManifest::New(),
                 payments::mojom::PaymentAppManifestError::
                     MANIFEST_STORAGE_OPERATION_FAILED);
    return;
  }

  payments::mojom::PaymentAppManifestPtr manifest =
      payments::mojom::PaymentAppManifest::New();
  manifest->label = manifest_proto.label();
  if (manifest_proto.has_icon())
    manifest->icon = manifest_proto.icon();
  for (const auto& option_proto : manifest_proto.options()) {
    payments::mojom::PaymentAppOptionPtr option =
        payments::mojom::PaymentAppOption::New();
    option->label = option_proto.label();
    if (option_proto.has_icon())
      option->icon = option_proto.icon();
    option->id = option_proto.id();
    for (const auto& method : option_proto.enabled_methods())
      option->enabled_methods.push_back(method);
    manifest->options.push_back(std::move(option));
  }

  callback.Run(std::move(manifest),
               payments::mojom::PaymentAppManifestError::NONE);
}

}  // namespace content
