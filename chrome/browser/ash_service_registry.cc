// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash_service_registry.h"

#include "ash/ash_service.h"
#include "ash/components/quick_launch/public/mojom/constants.mojom.h"
#include "ash/content/content_gpu_support.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "ash/shell.h"
#include "ash/ws/window_service_delegate_impl.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/chromeos/prefs/pref_connector_service.h"
#include "components/services/font/public/interfaces/constants.mojom.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "services/ui/ws2/window_service.h"
#include "ui/base/ui_base_features.h"

using content::ContentBrowserClient;

namespace ash_service_registry {
namespace {

struct Service {
  const char* name;
  const char* display_name;
  const char* process_group;  // If null, uses a separate process.
};

// Services shared between mash and non-mash configs.
constexpr Service kCommonServices[] = {
    {quick_launch::mojom::kServiceName, "Quick Launch", nullptr},
    {"autoclick_app", "Accessibility Autoclick", nullptr},
    {"touch_hud_app", "Touch HUD", nullptr},
    {font_service::mojom::kServiceName, "Font Service", nullptr},
};

// Services unique to mash. Note that the non-mash case also has an Ash service,
// it's just registered differently (see RegisterInProcessServices()).
constexpr Service kMashServices[] = {
    {ui::mojom::kServiceName, "UI Service", kAshAndUiProcessGroup},
    {ash::mojom::kServiceName, "Ash Window Manager and Shell",
     kAshAndUiProcessGroup},
};

void RegisterOutOfProcessServicesImpl(
    const Service* services,
    size_t num_services,
    ContentBrowserClient::OutOfProcessServiceMap* services_map) {
  for (size_t i = 0; i < num_services; ++i) {
    const Service& service = services[i];
    base::string16 display_name = base::ASCIIToUTF16(service.display_name);
    if (service.process_group) {
      (*services_map)[service.name] =
          ContentBrowserClient::OutOfProcessServiceInfo(display_name,
                                                        service.process_group);
    } else {
      (*services_map)[service.name] =
          ContentBrowserClient::OutOfProcessServiceInfo(display_name);
    }
  }
}

std::unique_ptr<service_manager::Service> CreateEmbeddedWindowService() {
  return std::make_unique<ui::ws2::WindowService>(
      ash::Shell::Get()->window_service_delegate(),
      std::make_unique<ash::ContentGpuSupport>());
}

}  // namespace

void RegisterOutOfProcessServices(
    ContentBrowserClient::OutOfProcessServiceMap* services) {
  if (base::FeatureList::IsEnabled(features::kMash)) {
    RegisterOutOfProcessServicesImpl(kCommonServices,
                                     base::size(kCommonServices), services);
    RegisterOutOfProcessServicesImpl(kMashServices, base::size(kMashServices),
                                     services);
  } else {
    RegisterOutOfProcessServicesImpl(kCommonServices,
                                     base::size(kCommonServices), services);
  }
}

void RegisterInProcessServices(
    content::ContentBrowserClient::StaticServiceMap* services) {
  {
    service_manager::EmbeddedServiceInfo info;
    info.factory =
        base::BindRepeating([]() -> std::unique_ptr<service_manager::Service> {
          return std::make_unique<AshPrefConnector>();
        });
    info.task_runner = base::ThreadTaskRunnerHandle::Get();
    (*services)[ash::mojom::kPrefConnectorServiceName] = info;
  }

  if (base::FeatureList::IsEnabled(features::kMash))
    return;

  (*services)[ash::mojom::kServiceName] =
      ash::AshService::CreateEmbeddedServiceInfo();

  service_manager::EmbeddedServiceInfo ui_service_info;
  ui_service_info.factory = base::BindRepeating(&CreateEmbeddedWindowService);
  ui_service_info.task_runner = base::ThreadTaskRunnerHandle::Get();
  (*services)[ui::mojom::kServiceName] = ui_service_info;
}

bool IsAshRelatedServiceName(const std::string& name) {
  for (const Service& service : kMashServices) {
    if (name == service.name)
      return true;
  }
  for (const Service& service : kCommonServices) {
    if (name == service.name)
      return true;
  }
  return false;
}

std::string GetAshRelatedServiceLabel(const std::string& service_name) {
  for (const Service& service : kMashServices) {
    if (service_name == service.name) {
      // Use the process group name when available because that makes it more
      // obvious that multiple services are running in the same process.
      return service.process_group ? service.process_group : service.name;
    }
  }
  for (const Service& service : kCommonServices) {
    if (service_name == service.name) {
      // Use the process group name when available because that makes it more
      // obvious that multiple services are running in the same process.
      return service.process_group ? service.process_group : service.name;
    }
  }
  return std::string();
}

bool ShouldTerminateOnServiceQuit(const std::string& name) {
  // Some services going down are treated as catastrophic failures, usually
  // because both the browser and the service cache data about each other's
  // state that is not rebuilt when the service restarts.
  return name == ui::mojom::kServiceName || name == ash::mojom::kServiceName;
}

}  // namespace ash_service_registry
