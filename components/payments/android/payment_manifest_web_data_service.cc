// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/android/payment_manifest_web_data_service.h"

#include "base/bind.h"
#include "base/location.h"

namespace payments {

PaymentManifestWebDataService::PaymentManifestWebDataService(
    scoped_refptr<WebDatabaseService> wdbs,
    const ProfileErrorCallback& callback,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner)
    : WebDataServiceBase(wdbs, callback, ui_task_runner) {}

PaymentManifestWebDataService::~PaymentManifestWebDataService() {}

void PaymentManifestWebDataService::AddPaymentWebAppManifest(
    std::vector<mojom::WebAppManifestSectionPtr> manifest) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::Bind(&PaymentManifestWebDataService::AddPaymentWebAppManifestImpl,
                 this, std::move(manifest)));
}

WebDatabase::State PaymentManifestWebDataService::AddPaymentWebAppManifestImpl(
    const std::vector<mojom::WebAppManifestSectionPtr>& manifest,
    WebDatabase* db) {
  if (WebAppManifestSectionTable::FromWebDatabase(db)->AddWebAppManifest(
          manifest)) {
    return WebDatabase::COMMIT_NEEDED;
  }

  return WebDatabase::COMMIT_NOT_NEEDED;
}

void PaymentManifestWebDataService::AddPaymentMethodManifest(
    const std::string& payment_method,
    std::vector<std::string> app_package_names) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::Bind(&PaymentManifestWebDataService::AddPaymentMethodManifestImpl,
                 this, payment_method, std::move(app_package_names)));
}

WebDatabase::State PaymentManifestWebDataService::AddPaymentMethodManifestImpl(
    const std::string& payment_method,
    const std::vector<std::string>& app_package_names,
    WebDatabase* db) {
  if (PaymentMethodManifestTable::FromWebDatabase(db)->AddManifest(
          payment_method, app_package_names)) {
    return WebDatabase::COMMIT_NEEDED;
  }

  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDataServiceBase::Handle
PaymentManifestWebDataService::GetPaymentWebAppManifest(
    const std::string& web_app,
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::Bind(&PaymentManifestWebDataService::GetPaymentWebAppManifestImpl,
                 this, web_app),
      consumer);
}

std::unique_ptr<WDTypedResult>
PaymentManifestWebDataService::GetPaymentWebAppManifestImpl(
    const std::string& web_app,
    WebDatabase* db) {
  RemoveExpiredData(db);
  return base::MakeUnique<
      WDResult<std::vector<mojom::WebAppManifestSectionPtr>>>(
      PAYMENT_WEB_APP_MANIFEST,
      WebAppManifestSectionTable::FromWebDatabase(db)->GetWebAppManifest(
          web_app));
}

WebDataServiceBase::Handle
PaymentManifestWebDataService::GetPaymentMethodManifest(
    const std::string& payment_method,
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::Bind(&PaymentManifestWebDataService::GetPaymentMethodManifestImpl,
                 this, payment_method),
      consumer);
}

std::unique_ptr<WDTypedResult>
PaymentManifestWebDataService::GetPaymentMethodManifestImpl(
    const std::string& payment_method,
    WebDatabase* db) {
  RemoveExpiredData(db);
  return base::MakeUnique<WDResult<std::vector<std::string>>>(
      PAYMENT_METHOD_MANIFEST,
      PaymentMethodManifestTable::FromWebDatabase(db)->GetManifest(
          payment_method));
}

void PaymentManifestWebDataService::RemoveExpiredData(WebDatabase* db) {
  PaymentMethodManifestTable::FromWebDatabase(db)->RemoveExpiredData();
  WebAppManifestSectionTable::FromWebDatabase(db)->RemoveExpiredData();
}

}  // namespace payments