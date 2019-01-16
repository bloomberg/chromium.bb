// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/arc_apps.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/arc_apps_factory.h"
#include "chrome/browser/apps/app_service/dip_px_util.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "ui/gfx/geometry/size.h"

namespace {

// ArcApps::LoadIcon runs a series of callbacks, defined here in back-to-front
// order so that e.g. the compiler knows LoadIcon2's signature when compiling
// LoadIcon1 (which binds LoadIcon2).
//
//  - LoadIcon0 is called back when the AppConnectionHolder is connected.
//  - LoadIcon1 is called back when the compressed (PNG) image is loaded.
//  - LoadIcon2 is called back when the uncompressed image is loaded.

void LoadIcon2(apps::mojom::Publisher::LoadIconCallback callback,
               const SkBitmap& bitmap) {
  apps::mojom::IconValuePtr iv = apps::mojom::IconValue::New();
  iv->icon_compression = apps::mojom::IconCompression::kUncompressed;
  iv->uncompressed = gfx::ImageSkia(gfx::ImageSkiaRep(bitmap, 0.0f));
  std::move(callback).Run(std::move(iv));
}

void LoadIcon1(apps::mojom::IconCompression icon_compression,
               apps::mojom::Publisher::LoadIconCallback callback,
               const std::vector<uint8_t>& icon_png_data) {
  switch (icon_compression) {
    case apps::mojom::IconCompression::kUnknown:
      std::move(callback).Run(apps::mojom::IconValue::New());
      break;

    case apps::mojom::IconCompression::kUncompressed:
      data_decoder::DecodeImage(
          content::ServiceManagerConnection::GetForProcess()->GetConnector(),
          icon_png_data, data_decoder::mojom::ImageCodec::DEFAULT, false,
          data_decoder::kDefaultMaxSizeInBytes, gfx::Size(),
          base::BindOnce(&LoadIcon2, std::move(callback)));
      break;

    case apps::mojom::IconCompression::kCompressed:
      apps::mojom::IconValuePtr iv = apps::mojom::IconValue::New();
      iv->icon_compression = apps::mojom::IconCompression::kCompressed;
      iv->compressed = icon_png_data;
      std::move(callback).Run(std::move(iv));
      break;
  }
}

void LoadIcon0(apps::mojom::IconCompression icon_compression,
               int size_hint_in_px,
               std::string package_name,
               std::string activity,
               std::string icon_resource_id,
               apps::mojom::Publisher::LoadIconCallback callback,
               apps::ArcApps::AppConnectionHolder* app_connection_holder) {
  if (icon_resource_id.empty()) {
    auto* app_instance =
        ARC_GET_INSTANCE_FOR_METHOD(app_connection_holder, RequestAppIcon);
    if (app_instance) {
      app_instance->RequestAppIcon(
          package_name, activity, size_hint_in_px,
          base::BindOnce(&LoadIcon1, icon_compression, std::move(callback)));
      return;
    }

  } else {
    auto* app_instance =
        ARC_GET_INSTANCE_FOR_METHOD(app_connection_holder, RequestShortcutIcon);
    if (app_instance) {
      app_instance->RequestShortcutIcon(
          icon_resource_id, size_hint_in_px,
          base::BindOnce(&LoadIcon1, icon_compression, std::move(callback)));
      return;
    }
  }

  // On failure, we still run the callback, with the zero IconValue.
  std::move(callback).Run(apps::mojom::IconValue::New());
}

}  // namespace

namespace apps {

// static
ArcApps* ArcApps::Get(Profile* profile) {
  return ArcAppsFactory::GetForProfile(profile);
}

ArcApps::ArcApps(Profile* profile)
    : binding_(this),
      profile_(profile),
      prefs_(ArcAppListPrefs::Get(profile)),
      next_u_key_(1) {
  if (!prefs_ || !arc::IsArcAllowedForProfile(profile_)) {
    return;
  }
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager) {
    return;
  }
  ObservePrefs();

  apps::mojom::PublisherPtr publisher;
  binding_.Bind(mojo::MakeRequest(&publisher));
  apps::AppServiceProxy::Get(profile)->AppService()->RegisterPublisher(
      std::move(publisher), apps::mojom::AppType::kArc);
}

ArcApps::~ArcApps() {
  if (prefs_) {
    prefs_->app_connection_holder()->RemoveObserver(this);
    prefs_->RemoveObserver(this);
  }
}

void ArcApps::Connect(apps::mojom::SubscriberPtr subscriber,
                      apps::mojom::ConnectOptionsPtr opts) {
  std::vector<apps::mojom::AppPtr> apps;
  if (prefs_) {
    for (const auto& app_id : prefs_->GetAppIds()) {
      std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
          prefs_->GetApp(app_id);
      if (app_info) {
        apps.push_back(Convert(app_id, *app_info));
      }
    }
  }
  subscriber->OnApps(std::move(apps));
  subscribers_.AddPtr(std::move(subscriber));
}

void ArcApps::LoadIcon(const std::string& app_id,
                       apps::mojom::IconKeyPtr icon_key,
                       apps::mojom::IconCompression icon_compression,
                       int32_t size_hint_in_dip,
                       LoadIconCallback callback) {
  if (prefs_ && !icon_key.is_null() &&
      (icon_key->icon_type == apps::mojom::IconType::kArc) &&
      !icon_key->s_key.empty()) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
        prefs_->GetApp(icon_key->s_key);
    if (app_info) {
      base::OnceCallback<void(apps::ArcApps::AppConnectionHolder*)> pending =
          base::BindOnce(&LoadIcon0, icon_compression,
                         ConvertDipToPx(size_hint_in_dip),
                         app_info->package_name, app_info->activity,
                         app_info->icon_resource_id, std::move(callback));

      AppConnectionHolder* app_connection_holder =
          prefs_->app_connection_holder();
      if (app_connection_holder->IsConnected()) {
        std::move(pending).Run(app_connection_holder);
      } else {
        pending_load_icon_calls_.push_back(std::move(pending));
      }
      return;
    }
  }

  // On failure, we still run the callback, with the zero IconValue.
  std::move(callback).Run(apps::mojom::IconValue::New());
}

void ArcApps::Launch(const std::string& app_id,
                     int32_t event_flags,
                     apps::mojom::LaunchSource launch_source,
                     int64_t display_id) {
  auto uit = arc::UserInteractionType::NOT_USER_INITIATED;
  switch (launch_source) {
    case apps::mojom::LaunchSource::kUnknown:
      return;
    case apps::mojom::LaunchSource::kFromAppListGrid:
      uit = arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER;
      break;
    case apps::mojom::LaunchSource::kFromAppListGridContextMenu:
      uit = arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER_CONTEXT_MENU;
      break;
    case apps::mojom::LaunchSource::kFromAppListQuery:
      uit = arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER_SEARCH;
      break;
    case apps::mojom::LaunchSource::kFromAppListQueryContextMenu:
      uit = arc::UserInteractionType::
          APP_STARTED_FROM_LAUNCHER_SEARCH_CONTEXT_MENU;
      break;
    case apps::mojom::LaunchSource::kFromAppListRecommendation:
      uit = arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER_SUGGESTED_APP;
      break;
  }

  arc::LaunchApp(profile_, app_id, event_flags, uit, display_id);
}

void ArcApps::SetPermission(const std::string& app_id,
                            apps::mojom::PermissionPtr permission) {
  NOTIMPLEMENTED();
}

void ArcApps::Uninstall(const std::string& app_id) {
  NOTIMPLEMENTED();
}

void ArcApps::OpenNativeSettings(const std::string& app_id) {
  NOTIMPLEMENTED();
}

void ArcApps::OnConnectionReady() {
  if (!prefs_) {
    prefs_ = ArcAppListPrefs::Get(profile_);
    ObservePrefs();
  }
  if (prefs_) {
    AppConnectionHolder* app_connection_holder =
        prefs_->app_connection_holder();
    for (auto& pending : pending_load_icon_calls_) {
      std::move(pending).Run(app_connection_holder);
    }
    pending_load_icon_calls_.clear();
  }
}

void ArcApps::ObservePrefs() {
  prefs_->AddObserver(this);
  prefs_->app_connection_holder()->AddObserver(this);
}

apps::mojom::AppPtr ArcApps::Convert(const std::string& app_id,
                                     const ArcAppListPrefs::AppInfo& app_info) {
  apps::mojom::AppPtr app = apps::mojom::App::New();

  app->app_type = apps::mojom::AppType::kArc;
  app->app_id = app_id;
  app->readiness = apps::mojom::Readiness::kReady;
  app->name = app_info.name;

  app->icon_key = apps::mojom::IconKey::New();
  app->icon_key->icon_type = apps::mojom::IconType::kArc;
  app->icon_key->s_key = app_id;
  app->icon_key->u_key = next_u_key_++;

  bool installed_internally =
      prefs_->IsDefault(app_id) ||
      prefs_->IsControlledByPolicy(app_info.package_name);
  app->installed_internally = installed_internally
                                  ? apps::mojom::OptionalBool::kTrue
                                  : apps::mojom::OptionalBool::kFalse;

  auto show = app_info.show_in_launcher ? apps::mojom::OptionalBool::kTrue
                                        : apps::mojom::OptionalBool::kFalse;
  app->show_in_launcher = show;
  app->show_in_search = show;

  return app;
}

}  // namespace apps
