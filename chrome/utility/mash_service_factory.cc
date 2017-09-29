// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/mash_service_factory.h"

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "mash/quick_launch/public/interfaces/constants.mojom.h"
#include "mash/quick_launch/quick_launch.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "services/ui/service.h"

#if defined(OS_CHROMEOS)
#include "ash/autoclick/mus/autoclick_application.h"  // nogncheck
#include "ash/mus/window_manager_application.h"       // nogncheck
#include "ash/public/interfaces/constants.mojom.h"    // nogncheck
#include "ash/touch_hud/mus/touch_hud_application.h"  // nogncheck
#endif                                                // defined(OS_CHROMEOS)

#if defined(OS_LINUX) && !defined(OS_ANDROID)
#include "components/font_service/font_service_app.h"
#include "components/font_service/public/interfaces/constants.mojom.h"
#endif  // defined(OS_LINUX) && !defined(OS_ANDROID)

namespace {

using ServiceFactoryFunction = std::unique_ptr<service_manager::Service>();

void RegisterMashService(
    content::ContentUtilityClient::StaticServiceMap* services,
    const std::string& name,
    ServiceFactoryFunction factory_function) {
  service_manager::EmbeddedServiceInfo service_info;
  service_info.factory = base::Bind(factory_function);
  services->emplace(name, service_info);
}

std::unique_ptr<service_manager::Service> CreateUiService() {
  return base::MakeUnique<ui::Service>();
}

#if defined(OS_CHROMEOS)
std::unique_ptr<service_manager::Service> CreateAshService() {
  const bool show_primary_host_on_connect = true;
  return base::MakeUnique<ash::mus::WindowManagerApplication>(
      show_primary_host_on_connect);
}

std::unique_ptr<service_manager::Service> CreateAccessibilityAutoclick() {
  return base::MakeUnique<ash::autoclick::AutoclickApplication>();
}

std::unique_ptr<service_manager::Service> CreateQuickLaunch() {
  return base::MakeUnique<mash::quick_launch::QuickLaunch>();
}

std::unique_ptr<service_manager::Service> CreateTouchHud() {
  return base::MakeUnique<ash::touch_hud::TouchHudApplication>();
}
#endif

#if defined(OS_LINUX) && !defined(OS_ANDROID)
std::unique_ptr<service_manager::Service> CreateFontService() {
  return base::MakeUnique<font_service::FontServiceApp>();
}

#endif  // defined(OS_LINUX) && !defined(OS_ANDROID)

}  // namespace

void RegisterMashServices(
    content::ContentUtilityClient::StaticServiceMap* services) {
  RegisterMashService(services, ui::mojom::kServiceName, &CreateUiService);
  RegisterMashService(services, mash::quick_launch::mojom::kServiceName,
                      &CreateQuickLaunch);
#if defined(OS_CHROMEOS)
  RegisterMashService(services, ash::mojom::kServiceName, &CreateAshService);
  RegisterMashService(services, "accessibility_autoclick",
                      &CreateAccessibilityAutoclick);
  RegisterMashService(services, "touch_hud", &CreateTouchHud);
#endif
#if defined(OS_LINUX) && !defined(OS_ANDROID)
  RegisterMashService(services, font_service::mojom::kServiceName,
                      &CreateFontService);
#endif  // defined(OS_LINUX) && !defined(OS_ANDROID)
}
