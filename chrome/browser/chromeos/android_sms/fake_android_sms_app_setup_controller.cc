// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/fake_android_sms_app_setup_controller.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_paths.h"

namespace chromeos {

namespace android_sms {

FakeAndroidSmsAppSetupController::AppMetadata::AppMetadata() = default;

FakeAndroidSmsAppSetupController::AppMetadata::AppMetadata(
    const AppMetadata& other) = default;

FakeAndroidSmsAppSetupController::AppMetadata::~AppMetadata() = default;

FakeAndroidSmsAppSetupController::FakeAndroidSmsAppSetupController() = default;

FakeAndroidSmsAppSetupController::~FakeAndroidSmsAppSetupController() = default;

const FakeAndroidSmsAppSetupController::AppMetadata*
FakeAndroidSmsAppSetupController::GetAppMetadataAtUrl(const GURL& url) const {
  const auto it = url_to_metadata_map_.find(url);
  if (it == url_to_metadata_map_.end())
    return nullptr;
  return std::addressof(it->second);
}

void FakeAndroidSmsAppSetupController::SetAppAtUrl(
    const GURL& url,
    const base::Optional<extensions::ExtensionId>& id_for_app) {
  if (!id_for_app) {
    url_to_metadata_map_.erase(url);
    return;
  }

  // Create a test Extension and add it to |url_to_metadata_map_|.
  base::FilePath path;
  base::PathService::Get(extensions::DIR_TEST_DATA, &path);
  url_to_metadata_map_[url].pwa =
      extensions::ExtensionBuilder(
          url.spec(), extensions::ExtensionBuilder::Type::PLATFORM_APP)
          .SetPath(path.AppendASCII(url.spec()))
          .SetID(*id_for_app)
          .Build();
}

void FakeAndroidSmsAppSetupController::CompletePendingSetUpAppRequest(
    const GURL& expected_url,
    const base::Optional<extensions::ExtensionId>& id_for_app) {
  DCHECK(!pending_set_up_app_requests_.empty());

  auto request = std::move(pending_set_up_app_requests_.front());
  pending_set_up_app_requests_.erase(pending_set_up_app_requests_.begin());
  DCHECK_EQ(expected_url, request->first);

  if (!id_for_app) {
    std::move(request->second).Run(false /* success */);
    return;
  }

  SetAppAtUrl(expected_url, *id_for_app);
  std::move(request->second).Run(true /* success */);
}

void FakeAndroidSmsAppSetupController::CompletePendingDeleteCookieRequest(
    const GURL& expected_url) {
  DCHECK(!pending_delete_cookie_requests_.empty());

  auto request = std::move(pending_delete_cookie_requests_.front());
  pending_delete_cookie_requests_.erase(
      pending_delete_cookie_requests_.begin());
  DCHECK_EQ(expected_url, request->first);

  // The app must exist before the cookie is deleted.
  auto it = url_to_metadata_map_.find(expected_url);
  DCHECK(it != url_to_metadata_map_.end());

  it->second.is_cookie_present = false;

  std::move(request->second).Run(true /* success */);
}

void FakeAndroidSmsAppSetupController::CompleteRemoveAppRequest(
    const GURL& expected_url,
    bool should_succeed) {
  DCHECK(!pending_remove_app_requests_.empty());

  auto request = std::move(pending_remove_app_requests_.front());
  pending_remove_app_requests_.erase(pending_remove_app_requests_.begin());
  DCHECK_EQ(expected_url, request->first);

  if (should_succeed)
    SetAppAtUrl(expected_url, base::nullopt /* id_for_app */);

  std::move(request->second).Run(should_succeed);
}

void FakeAndroidSmsAppSetupController::SetUpApp(const GURL& url,
                                                SuccessCallback callback) {
  pending_set_up_app_requests_.push_back(
      std::make_unique<RequestData>(url, std::move(callback)));
}

const extensions::Extension* FakeAndroidSmsAppSetupController::GetPwa(
    const GURL& url) {
  auto it = url_to_metadata_map_.find(url);
  if (it == url_to_metadata_map_.end())
    return nullptr;
  return it->second.pwa.get();
}

void FakeAndroidSmsAppSetupController::DeleteRememberDeviceByDefaultCookie(
    const GURL& url,
    SuccessCallback callback) {
  pending_delete_cookie_requests_.push_back(
      std::make_unique<RequestData>(url, std::move(callback)));
}

void FakeAndroidSmsAppSetupController::RemoveApp(const GURL& url,
                                                 SuccessCallback callback) {
  pending_remove_app_requests_.push_back(
      std::make_unique<RequestData>(url, std::move(callback)));
}

}  // namespace android_sms

}  // namespace chromeos
