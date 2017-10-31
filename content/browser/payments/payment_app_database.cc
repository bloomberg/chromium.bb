// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/payment_app_database.h"

#include <map>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/browser/payments/payment_app.pb.h"
#include "content/browser/payments/payment_app_context_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/manifest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

using ::payments::mojom::PaymentHandlerStatus;
using ::payments::mojom::PaymentInstrument;
using ::payments::mojom::PaymentInstrumentPtr;

const char kPaymentAppPrefix[] = "PaymentApp:";
const char kPaymentInstrumentPrefix[] = "PaymentInstrument:";
const char kPaymentInstrumentKeyInfoPrefix[] = "PaymentInstrumentKeyInfo:";

// |pattern| is the scope URL of the service worker registration.
std::string CreatePaymentAppKey(const std::string& pattern) {
  return kPaymentAppPrefix + pattern;
}

std::string CreatePaymentInstrumentKey(const std::string& instrument_key) {
  return kPaymentInstrumentPrefix + instrument_key;
}

std::string CreatePaymentInstrumentKeyInfoKey(
    const std::string& instrument_key) {
  return kPaymentInstrumentKeyInfoPrefix + instrument_key;
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
  for (const auto& icon_proto : instrument_proto.icons()) {
    Manifest::Icon icon;
    icon.src = GURL(icon_proto.src());
    icon.type = base::UTF8ToUTF16(icon_proto.type());
    for (const auto& size_proto : icon_proto.sizes()) {
      icon.sizes.emplace_back(size_proto.width(), size_proto.height());
    }
    instrument->icons.emplace_back(icon);
  }
  for (const auto& method : instrument_proto.enabled_methods())
    instrument->enabled_methods.push_back(method);
  instrument->stringified_capabilities =
      instrument_proto.stringified_capabilities();

  return instrument;
}

std::unique_ptr<StoredPaymentApp> ToStoredPaymentApp(const std::string& input) {
  StoredPaymentAppProto app_proto;
  if (!app_proto.ParseFromString(input))
    return std::unique_ptr<StoredPaymentApp>();

  std::unique_ptr<StoredPaymentApp> app = std::make_unique<StoredPaymentApp>();
  app->registration_id = app_proto.registration_id();
  app->scope = GURL(app_proto.scope());
  app->name = app_proto.name();
  app->prefer_related_applications = app_proto.prefer_related_applications();
  for (const auto& related_app : app_proto.related_applications()) {
    app->related_applications.emplace_back(StoredRelatedApplication());
    app->related_applications.back().platform = related_app.platform();
    app->related_applications.back().id = related_app.id();
  }
  app->user_hint = app_proto.user_hint();

  if (!app_proto.icon().empty()) {
    std::string icon_raw_data;
    base::Base64Decode(app_proto.icon(), &icon_raw_data);
    // Note that the icon has been decoded to PNG raw data regardless of the
    // original icon format that was downloaded.
    gfx::Image icon_image = gfx::Image::CreateFrom1xPNGBytes(
        reinterpret_cast<const unsigned char*>(icon_raw_data.data()),
        icon_raw_data.size());
    app->icon = std::make_unique<SkBitmap>(icon_image.AsBitmap());
  }

  return app;
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

void PaymentAppDatabase::ReadAllPaymentApps(
    ReadAllPaymentAppsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->GetUserDataForAllRegistrationsByKeyPrefix(
      kPaymentAppPrefix, base::Bind(&PaymentAppDatabase::DidReadAllPaymentApps,
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

  if (instrument->icons.size() > 0) {
    std::vector<Manifest::Icon> icons(instrument->icons);
    PaymentInstrumentIconFetcher::Start(
        scope, service_worker_context_->GetProviderHostIds(scope.GetOrigin()),
        icons,
        base::BindOnce(&PaymentAppDatabase::DidFetchedPaymentInstrumentIcon,
                       weak_ptr_factory_.GetWeakPtr(), scope, instrument_key,
                       base::Passed(std::move(instrument)),
                       base::Passed(std::move(callback))));
  } else {
    service_worker_context_->FindReadyRegistrationForPattern(
        scope,
        base::Bind(
            &PaymentAppDatabase::DidFindRegistrationToWritePaymentInstrument,
            weak_ptr_factory_.GetWeakPtr(), instrument_key,
            base::Passed(std::move(instrument)), std::string(),
            base::Passed(std::move(callback))));
  }
}

void PaymentAppDatabase::DidFetchedPaymentInstrumentIcon(
    const GURL& scope,
    const std::string& instrument_key,
    payments::mojom::PaymentInstrumentPtr instrument,
    WritePaymentInstrumentCallback callback,
    const std::string& icon) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (icon.empty()) {
    std::move(callback).Run(PaymentHandlerStatus::FETCH_INSTRUMENT_ICON_FAILED);
    return;
  }

  service_worker_context_->FindReadyRegistrationForPattern(
      scope,
      base::Bind(
          &PaymentAppDatabase::DidFindRegistrationToWritePaymentInstrument,
          weak_ptr_factory_.GetWeakPtr(), instrument_key,
          base::Passed(std::move(instrument)), icon,
          base::Passed(std::move(callback))));
}

void PaymentAppDatabase::FetchAndWritePaymentAppInfo(
    const GURL& context,
    const GURL& scope,
    const std::string& user_hint,
    FetchAndWritePaymentAppInfoCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  payment_app_info_fetcher_ = new PaymentAppInfoFetcher();
  payment_app_info_fetcher_->Start(
      context, service_worker_context_,
      base::BindOnce(&PaymentAppDatabase::FetchPaymentAppInfoCallback,
                     weak_ptr_factory_.GetWeakPtr(), scope, user_hint,
                     std::move(callback)));
}

void PaymentAppDatabase::FetchPaymentAppInfoCallback(
    const GURL& scope,
    const std::string& user_hint,
    FetchAndWritePaymentAppInfoCallback callback,
    std::unique_ptr<PaymentAppInfoFetcher::PaymentAppInfo> app_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  payment_app_info_fetcher_ = nullptr;

  service_worker_context_->FindReadyRegistrationForPattern(
      scope,
      base::Bind(&PaymentAppDatabase::DidFindRegistrationToWritePaymentAppInfo,
                 weak_ptr_factory_.GetWeakPtr(), user_hint,
                 base::Passed(std::move(callback)),
                 base::Passed(std::move(app_info))));
}

void PaymentAppDatabase::DidFindRegistrationToWritePaymentAppInfo(
    const std::string& user_hint,
    FetchAndWritePaymentAppInfoCallback callback,
    std::unique_ptr<PaymentAppInfoFetcher::PaymentAppInfo> app_info,
    ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status != SERVICE_WORKER_OK) {
    std::move(callback).Run(PaymentHandlerStatus::NO_ACTIVE_WORKER);
    return;
  }

  StoredPaymentAppProto payment_app_proto;
  payment_app_proto.set_registration_id(registration->id());
  payment_app_proto.set_scope(registration->pattern().spec());
  payment_app_proto.set_name(
      app_info->name.empty()
          ? GURL(payment_app_proto.scope()).GetOrigin().spec()
          : app_info->name);
  payment_app_proto.set_icon(app_info->icon);
  payment_app_proto.set_prefer_related_applications(
      app_info->prefer_related_applications);
  for (const auto& related_app : app_info->related_applications) {
    StoredRelatedApplicationProto* related_app_proto =
        payment_app_proto.add_related_applications();
    related_app_proto->set_platform(related_app.platform);
    related_app_proto->set_id(related_app.id);
  }
  payment_app_proto.set_user_hint(user_hint);

  std::string serialized_payment_app;
  bool success = payment_app_proto.SerializeToString(&serialized_payment_app);
  DCHECK(success);

  service_worker_context_->StoreRegistrationUserData(
      registration->id(), registration->pattern().GetOrigin(),
      {{CreatePaymentAppKey(registration->pattern().spec()),
        serialized_payment_app}},
      base::Bind(&PaymentAppDatabase::DidWritePaymentApp,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(std::move(callback)),
                 app_info->name.empty() | app_info->icon.empty()));
}

void PaymentAppDatabase::DidWritePaymentApp(
    FetchAndWritePaymentAppInfoCallback callback,
    bool fetch_app_info_failed,
    ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  PaymentHandlerStatus handler_status =
      fetch_app_info_failed
          ? PaymentHandlerStatus::FETCH_PAYMENT_APP_INFO_FAILED
          : PaymentHandlerStatus::SUCCESS;
  handler_status = status == SERVICE_WORKER_OK
                       ? handler_status
                       : PaymentHandlerStatus::STORAGE_OPERATION_FAILED;
  return std::move(callback).Run(handler_status);
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

void PaymentAppDatabase::SetPaymentAppUserHint(const GURL& scope,
                                               const std::string& user_hint) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->FindReadyRegistrationForPattern(
      scope,
      base::Bind(
          &PaymentAppDatabase::DidFindRegistrationToSetPaymentAppUserHint,
          weak_ptr_factory_.GetWeakPtr(), user_hint));
}

void PaymentAppDatabase::DidFindRegistrationToSetPaymentAppUserHint(
    const std::string& user_hint,
    ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status != SERVICE_WORKER_OK)
    return;

  service_worker_context_->GetRegistrationUserDataByKeyPrefix(
      registration->id(), CreatePaymentAppKey(registration->pattern().spec()),
      base::Bind(&PaymentAppDatabase::DidGetPaymentAppInfoToSetUserHint,
                 weak_ptr_factory_.GetWeakPtr(), user_hint, registration->id(),
                 registration->pattern()));
}

void PaymentAppDatabase::DidGetPaymentAppInfoToSetUserHint(
    const std::string& user_hint,
    int64_t registration_id,
    const GURL& pattern,
    const std::vector<std::string>& data,
    ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status != SERVICE_WORKER_OK)
    return;

  DCHECK_LE(data.size(), 1U);
  StoredPaymentAppProto app_proto;
  if (data.size() == 1U) {
    app_proto.ParseFromString(data[0]);
  }
  app_proto.set_user_hint(user_hint);

  std::string serialized_payment_app;
  bool success = app_proto.SerializeToString(&serialized_payment_app);
  DCHECK(success);

  service_worker_context_->StoreRegistrationUserData(
      registration_id, pattern.GetOrigin(),
      {{CreatePaymentAppKey(pattern.spec()), serialized_payment_app}},
      base::Bind(&PaymentAppDatabase::DidSetPaymentAppUserHint,
                 weak_ptr_factory_.GetWeakPtr()));
}

void PaymentAppDatabase::DidSetPaymentAppUserHint(
    ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(status == SERVICE_WORKER_OK);
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
    std::unique_ptr<StoredPaymentApp> app =
        ToStoredPaymentApp(item_of_raw_data.second);
    if (app)
      apps[app->registration_id] = std::move(app);
  }

  if (apps.size() == 0U) {
    std::move(callback).Run(PaymentApps());
    return;
  }

  service_worker_context_->GetUserDataForAllRegistrationsByKeyPrefix(
      kPaymentInstrumentPrefix,
      base::Bind(&PaymentAppDatabase::DidReadAllPaymentInstruments,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(std::move(apps)),
                 base::Passed(std::move(callback))));
}

void PaymentAppDatabase::DidReadAllPaymentInstruments(
    PaymentApps apps,
    ReadAllPaymentAppsCallback callback,
    const std::vector<std::pair<int64_t, std::string>>& raw_data,
    ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status != SERVICE_WORKER_OK) {
    std::move(callback).Run(std::move(apps));
    return;
  }

  for (const auto& item_of_raw_data : raw_data) {
    StoredPaymentInstrumentProto instrument_proto;
    if (!instrument_proto.ParseFromString(item_of_raw_data.second))
      continue;

    int64_t id = instrument_proto.registration_id();
    if (!base::ContainsKey(apps, id))
      continue;

    for (const auto& method : instrument_proto.enabled_methods()) {
      apps[id]->enabled_methods.push_back(method);
    }

    apps[id]->capabilities.emplace_back(StoredCapabilities());
    for (const auto& network : instrument_proto.supported_card_networks()) {
      apps[id]->capabilities.back().supported_card_networks.emplace_back(
          network);
    }
    for (const auto& type : instrument_proto.supported_card_types()) {
      apps[id]->capabilities.back().supported_card_types.emplace_back(type);
    }
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
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(std::move(callback))));
}

void PaymentAppDatabase::DidHasPaymentInstrument(
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
    const std::string& decoded_instrument_icon,
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
  instrument_proto.set_decoded_instrument_icon(decoded_instrument_icon);
  instrument_proto.set_instrument_key(instrument_key);
  instrument_proto.set_name(instrument->name);
  for (const auto& method : instrument->enabled_methods) {
    instrument_proto.add_enabled_methods(method);
  }
  for (const auto& icon : instrument->icons) {
    StoredPaymentInstrumentImageObject* image_object_proto =
        instrument_proto.add_icons();
    image_object_proto->set_src(icon.src.spec());
    image_object_proto->set_type(base::UTF16ToUTF8(icon.type));
    for (const auto& size : icon.sizes) {
      ImageSizeProto* size_proto = image_object_proto->add_sizes();
      size_proto->set_width(size.width());
      size_proto->set_height(size.height());
    }
  }
  instrument_proto.set_stringified_capabilities(
      instrument->stringified_capabilities);
  for (const auto& network : instrument->supported_networks) {
    instrument_proto.add_supported_card_networks(static_cast<int32_t>(network));
  }
  for (const auto& type : instrument->supported_types) {
    instrument_proto.add_supported_card_types(static_cast<int32_t>(type));
  }

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
                     weak_ptr_factory_.GetWeakPtr(), std::move(registration),
                     std::move(callback)));
}

void PaymentAppDatabase::DidGetKeysToClearPaymentInstruments(
    scoped_refptr<ServiceWorkerRegistration> registration,
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

  // Clear payment app info after clearing all payment instruments.
  keys_with_prefix.push_back(
      CreatePaymentAppKey(registration->pattern().spec()));

  service_worker_context_->ClearRegistrationUserData(
      registration->id(), keys_with_prefix,
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
