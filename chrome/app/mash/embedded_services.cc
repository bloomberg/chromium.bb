// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/mash/embedded_services.h"

#include "mash/catalog_viewer/catalog_viewer.h"
#include "mash/catalog_viewer/public/interfaces/constants.mojom.h"
#include "mash/common/config.h"
#include "mash/quick_launch/public/interfaces/constants.mojom.h"
#include "mash/quick_launch/quick_launch.h"
#include "mash/session/public/interfaces/constants.mojom.h"
#include "mash/session/session.h"
#include "mash/task_viewer/public/interfaces/constants.mojom.h"
#include "mash/task_viewer/task_viewer.h"
#include "services/ui/ime/test_ime_driver/test_ime_application.h"
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
#endif  // defined(OS_LINUX) && !defined(OS_ANDROID)

std::unique_ptr<service_manager::Service> CreateEmbeddedMashService(
    const std::string& service_name) {
#if defined(OS_CHROMEOS)
  if (service_name == ash::mojom::kServiceName) {
    const bool show_primary_host_on_connect = true;
    return base::WrapUnique(
        new ash::mus::WindowManagerApplication(show_primary_host_on_connect));
  }
  if (service_name == "accessibility_autoclick")
    return base::MakeUnique<ash::autoclick::AutoclickApplication>();
  if (service_name == "touch_hud")
    return base::MakeUnique<ash::touch_hud::TouchHudApplication>();
#endif  // defined(OS_CHROMEOS)
  if (service_name == mash::catalog_viewer::mojom::kServiceName)
    return base::MakeUnique<mash::catalog_viewer::CatalogViewer>();
  if (service_name == mash::session::mojom::kServiceName)
    return base::MakeUnique<mash::session::Session>();
  if (service_name == ui::mojom::kServiceName)
    return base::MakeUnique<ui::Service>();
  if (service_name == mash::quick_launch::mojom::kServiceName)
    return base::MakeUnique<mash::quick_launch::QuickLaunch>();
  if (service_name == mash::task_viewer::mojom::kServiceName)
    return base::MakeUnique<mash::task_viewer::TaskViewer>();
  if (service_name == "test_ime_driver")
    return base::MakeUnique<ui::test::TestIMEApplication>();
#if defined(OS_LINUX) && !defined(OS_ANDROID)
  if (service_name == "font_service")
    return base::MakeUnique<font_service::FontServiceApp>();
#endif  // defined(OS_LINUX) && !defined(OS_ANDROID)

  return nullptr;
}
