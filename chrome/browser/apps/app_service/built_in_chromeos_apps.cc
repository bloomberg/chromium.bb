// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/built_in_chromeos_apps.h"

#include <utility>
#include <vector>

#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/internal_app/internal_app_item.h"
#include "chrome/browser/ui/app_list/internal_app/internal_app_metadata.h"
#include "chrome/browser/ui/app_list/search/internal_app_result.h"
#include "chrome/browser/ui/app_list/search/search_util.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

apps::mojom::AppPtr Convert(const app_list::InternalApp& internal_app) {
  if ((internal_app.app_id == nullptr) ||
      (internal_app.name_string_resource_id == 0) ||
      (internal_app.icon_resource_id <= 0)) {
    return apps::mojom::AppPtr();
  }
  apps::mojom::AppPtr app = apps::mojom::App::New();

  app->app_type = apps::mojom::AppType::kBuiltIn;
  app->app_id = internal_app.app_id;
  app->readiness = apps::mojom::Readiness::kReady;
  app->name = l10n_util::GetStringUTF8(internal_app.name_string_resource_id);

  app->icon_key = apps::mojom::IconKey::New();
  app->icon_key->icon_type = apps::mojom::IconType::kResource;
  app->icon_key->u_key = static_cast<uint64_t>(internal_app.icon_resource_id);

  app->installed_internally = apps::mojom::OptionalBool::kTrue;
  app->show_in_launcher = internal_app.show_in_launcher
                              ? apps::mojom::OptionalBool::kTrue
                              : apps::mojom::OptionalBool::kFalse;
  app->show_in_search = internal_app.searchable
                            ? apps::mojom::OptionalBool::kTrue
                            : apps::mojom::OptionalBool::kFalse;

  return app;
}

}  // namespace

namespace apps {

BuiltInChromeOsApps::BuiltInChromeOsApps()
    : binding_(this), profile_(nullptr) {}

BuiltInChromeOsApps::~BuiltInChromeOsApps() = default;

void BuiltInChromeOsApps::Initialize(
    const apps::mojom::AppServicePtr& app_service,
    Profile* profile) {
  apps::mojom::PublisherPtr publisher;
  binding_.Bind(mojo::MakeRequest(&publisher));
  app_service->RegisterPublisher(std::move(publisher),
                                 apps::mojom::AppType::kBuiltIn);

  profile_ = profile;
}

void BuiltInChromeOsApps::Connect(apps::mojom::SubscriberPtr subscriber,
                                  apps::mojom::ConnectOptionsPtr opts) {
  std::vector<apps::mojom::AppPtr> apps;
  if (profile_) {
    // TODO(crbug.com/826982): move source of truth for built-in apps from
    // ui/app_list to here when the AppService feature is enabled by default.
    for (const auto& internal_app : app_list::GetInternalAppList(profile_)) {
      // TODO(crbug.com/826982): support the "continue reading" app? Or leave
      // it specifically to the chrome/browser/ui/app_list/search code? If
      // moved here, it might mean calling sync_sessions::SessionSyncService's
      // SubscribeToForeignSessionsChanged. See also app_search_provider.cc's
      // InternalDataSource.
      if (internal_app.internal_app_name ==
          app_list::InternalAppName::kContinueReading) {
        continue;
      }

      apps::mojom::AppPtr app = Convert(internal_app);
      if (!app.is_null()) {
        apps.push_back(std::move(app));
      }
    }
  }
  subscriber->OnApps(std::move(apps));

  // Unlike other apps::mojom::Publisher implementations, we don't need to
  // retain the subscriber (e.g. add it to a
  // mojo::InterfacePtrSet<apps::mojom::Subscriber> subscribers_) after this
  // function returns. The list of built-in Chrome OS apps is fixed for the
  // lifetime of the Chrome OS session. There won't be any further updates.
}

void BuiltInChromeOsApps::LoadIcon(
    const std::string& app_id,
    apps::mojom::IconKeyPtr icon_key,
    apps::mojom::IconCompression icon_compression,
    int32_t size_hint_in_dip,
    LoadIconCallback callback) {
  if (!icon_key.is_null() &&
      (icon_key->icon_type == apps::mojom::IconType::kResource) &&
      (icon_key->u_key != 0) && (icon_key->u_key <= INT_MAX)) {
    int resource_id = static_cast<int>(icon_key->u_key);
    LoadIconFromResource(icon_compression, size_hint_in_dip, resource_id,
                         std::move(callback));
    return;
  }
  // On failure, we still run the callback, with the zero IconValue.
  std::move(callback).Run(apps::mojom::IconValue::New());
}

void BuiltInChromeOsApps::Launch(const std::string& app_id,
                                 int32_t event_flags,
                                 apps::mojom::LaunchSource launch_source,
                                 int64_t display_id) {
  switch (launch_source) {
    case apps::mojom::LaunchSource::kUnknown:
      break;
    case apps::mojom::LaunchSource::kFromAppListGrid:
    case apps::mojom::LaunchSource::kFromAppListGridContextMenu:
      InternalAppItem::RecordActiveHistogram(app_id);
      break;
    case apps::mojom::LaunchSource::kFromAppListQuery:
    case apps::mojom::LaunchSource::kFromAppListQueryContextMenu:
      app_list::RecordHistogram(app_list::APP_SEARCH_RESULT);
      app_list::InternalAppResult::RecordOpenHistogram(app_id);
      break;
    case apps::mojom::LaunchSource::kFromAppListRecommendation:
      app_list::InternalAppResult::RecordOpenHistogram(app_id);
      break;
  }

  app_list::OpenInternalApp(app_id, profile_, event_flags);
}

void BuiltInChromeOsApps::SetPermission(const std::string& app_id,
                                        apps::mojom::PermissionPtr permission) {
  NOTIMPLEMENTED();
}

void BuiltInChromeOsApps::Uninstall(const std::string& app_id) {
  NOTIMPLEMENTED();
}

void BuiltInChromeOsApps::OpenNativeSettings(const std::string& app_id) {
  NOTIMPLEMENTED();
}

}  // namespace apps
