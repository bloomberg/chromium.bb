// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mash_service_registry.h"

#include "ash/public/interfaces/constants.mojom.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/font_service/public/interfaces/constants.mojom.h"
#include "mash/quick_launch/public/interfaces/constants.mojom.h"
#include "mash/quick_launch/quick_launch.h"
#include "services/ui/public/interfaces/constants.mojom.h"

namespace mash_service_registry {
namespace {

struct Service {
  const char* name;
  const char* description;
};

constexpr Service kServices[] = {
    {mash::quick_launch::mojom::kServiceName, "Quick Launch"},
    {ui::mojom::kServiceName, "UI Service"},
    {ash::mojom::kServiceName, "Ash Window Manager and Shell"},
    {"accessibility_autoclick", "Ash Accessibility Autoclick"},
    {"touch_hud", "Ash Touch Hud"},
    {font_service::mojom::kServiceName, "Font Service"},
};

}  // namespace

void RegisterOutOfProcessServices(
    content::ContentBrowserClient::OutOfProcessServiceMap* services) {
  for (size_t i = 0; i < arraysize(kServices); ++i) {
    (*services)[kServices[i].name] =
        base::ASCIIToUTF16(kServices[i].description);
  }
}

bool IsMashServiceName(const std::string& name) {
  for (size_t i = 0; i < arraysize(kServices); ++i) {
    if (name == kServices[i].name)
      return true;
  }
  return false;
}

bool ShouldTerminateOnServiceQuit(const std::string& name) {
  // Some services going down are treated as catastrophic failures, usually
  // because both the browser and the service cache data about each other's
  // state that is not rebuilt when the service restarts.
  return name == ui::mojom::kServiceName || name == ash::mojom::kServiceName;
}

}  // namespace mash_service_registry
