// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mash_service_registry.h"

#include "ash/components/quick_launch/public/mojom/constants.mojom.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/services/font/public/interfaces/constants.mojom.h"
#include "services/ui/public/interfaces/constants.mojom.h"

using content::ContentBrowserClient;

namespace mash_service_registry {
namespace {

struct Service {
  const char* name;
  const char* display_name;
  const char* process_group;  // If null, uses a separate process.
};

constexpr Service kServices[] = {
    {quick_launch::mojom::kServiceName, "Quick Launch", nullptr},
    {ui::mojom::kServiceName, "UI Service", kAshAndUiProcessGroup},
    {ash::mojom::kServiceName, "Ash Window Manager and Shell",
     kAshAndUiProcessGroup},
    {"autoclick_app", "Accessibility Autoclick", nullptr},
    {"touch_hud_app", "Touch HUD", nullptr},
    {font_service::mojom::kServiceName, "Font Service", nullptr},
};

}  // namespace

void RegisterOutOfProcessServices(
    ContentBrowserClient::OutOfProcessServiceMap* services) {
  for (const auto& service : kServices) {
    base::string16 display_name = base::ASCIIToUTF16(service.display_name);
    if (service.process_group) {
      (*services)[service.name] = ContentBrowserClient::OutOfProcessServiceInfo(
          display_name, service.process_group);
    } else {
      (*services)[service.name] =
          ContentBrowserClient::OutOfProcessServiceInfo(display_name);
    }
  }
}

bool IsMashServiceName(const std::string& name) {
  for (size_t i = 0; i < arraysize(kServices); ++i) {
    if (name == kServices[i].name)
      return true;
  }
  return false;
}

std::string GetMashServiceLabel(const std::string& service_name) {
  for (const Service& service : kServices) {
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

}  // namespace mash_service_registry
