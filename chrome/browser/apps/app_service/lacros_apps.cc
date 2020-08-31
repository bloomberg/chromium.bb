// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/lacros_apps.h"

#include <utility>

#include "base/bind.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/chromeos/lacros/lacros_loader.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "chromeos/constants/chromeos_features.h"
#include "extensions/common/constants.h"

namespace apps {

LacrosApps::LacrosApps(
    const mojo::Remote<apps::mojom::AppService>& app_service) {
  DCHECK(chromeos::features::IsLacrosSupportEnabled());
  PublisherBase::Initialize(app_service, apps::mojom::AppType::kLacros);
}

LacrosApps::~LacrosApps() = default;

apps::mojom::AppPtr LacrosApps::GetLacrosApp(bool is_ready) {
  apps::mojom::AppPtr app = apps::PublisherBase::MakeApp(
      apps::mojom::AppType::kLacros, extension_misc::kLacrosAppId,
      apps::mojom::Readiness::kReady,
      "LaCrOS",  // TODO(jamescook): Localized name.
      apps::mojom::InstallSource::kSystem);
  app->icon_key = NewIconKey(is_ready ? State::kReady : State::kLoading);
  app->searchable = apps::mojom::OptionalBool::kTrue;
  app->show_in_launcher = apps::mojom::OptionalBool::kTrue;
  app->show_in_search = apps::mojom::OptionalBool::kTrue;
  app->show_in_management = apps::mojom::OptionalBool::kFalse;
  return app;
}

apps::mojom::IconKeyPtr LacrosApps::NewIconKey(State state) {
  // Show different icons based on download state.
  apps::IconEffects icon_effects;
  switch (state) {
    case State::kLoading:
      icon_effects = apps::IconEffects::kPaused;
      break;
    case State::kError:
      icon_effects = apps::IconEffects::kBlocked;
      break;
    case State::kReady:
      icon_effects = apps::IconEffects::kNone;
      break;
  }
  apps::mojom::IconKeyPtr icon_key =
      icon_key_factory_.MakeIconKey(icon_effects);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Canary icon only exists in branded builds.
  icon_key->resource_id = IDR_PRODUCT_LOGO_256_CANARY;
#else
  icon_key->resource_id = IDR_PRODUCT_LOGO_256;
#endif
  return icon_key;
}

void LacrosApps::Connect(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  bool is_ready = LacrosLoader::Get()->IsReady();
  if (!is_ready) {
    LacrosLoader::Get()->SetLoadCompleteCallback(base::BindOnce(
        &LacrosApps::OnLoadComplete, weak_factory_.GetWeakPtr()));
  }
  std::vector<apps::mojom::AppPtr> apps;
  apps.push_back(GetLacrosApp(is_ready));

  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));
  subscriber->OnApps(std::move(apps));
  subscribers_.Add(std::move(subscriber));
}

void LacrosApps::LoadIcon(const std::string& app_id,
                          apps::mojom::IconKeyPtr icon_key,
                          apps::mojom::IconCompression icon_compression,
                          int32_t size_hint_in_dip,
                          bool allow_placeholder_icon,
                          LoadIconCallback callback) {
  if (icon_key &&
      icon_key->resource_id != apps::mojom::IconKey::kInvalidResourceId) {
    LoadIconFromResource(
        icon_compression, size_hint_in_dip, icon_key->resource_id,
        /*is_placeholder_icon=*/false,
        static_cast<IconEffects>(icon_key->icon_effects), std::move(callback));
    return;
  }
  // On failure, we still run the callback, with the zero IconValue.
  std::move(callback).Run(apps::mojom::IconValue::New());
}

void LacrosApps::Launch(const std::string& app_id,
                        int32_t event_flags,
                        apps::mojom::LaunchSource launch_source,
                        int64_t display_id) {
  DCHECK_EQ(extension_misc::kLacrosAppId, app_id);
  LacrosLoader::Get()->Start();
}

void LacrosApps::GetMenuModel(const std::string& app_id,
                              apps::mojom::MenuType menu_type,
                              int64_t display_id,
                              GetMenuModelCallback callback) {
  // No menu items.
  std::move(callback).Run(apps::mojom::MenuItems::New());
}

void LacrosApps::OnLoadComplete(bool success) {
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kLacros;
  app->app_id = extension_misc::kLacrosAppId;
  app->icon_key = NewIconKey(success ? State::kReady : State::kError);
  Publish(std::move(app), subscribers_);
}

}  // namespace apps
