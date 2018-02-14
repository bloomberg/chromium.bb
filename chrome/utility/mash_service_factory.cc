// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/mash_service_factory.h"

#include <memory>

#include "ash/autoclick/mus/autoclick_application.h"
#include "ash/components/quick_launch/public/mojom/constants.mojom.h"
#include "ash/components/quick_launch/quick_launch.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "ash/touch_hud/mus/touch_hud_application.h"
#include "ash/window_manager_service.h"
#include "base/bind.h"
#include "build/build_config.h"
#include "components/font_service/font_service_app.h"
#include "components/font_service/public/interfaces/constants.mojom.h"
#include "services/ui/common/image_cursors_set.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "services/ui/service.h"

namespace {

using ServiceFactoryFunction = std::unique_ptr<service_manager::Service>();

void RegisterMashService(
    content::ContentUtilityClient::StaticServiceMap* services,
    const std::string& name,
    ServiceFactoryFunction factory_function) {
  service_manager::EmbeddedServiceInfo service_info;
  service_info.factory = base::BindRepeating(factory_function);
  services->emplace(name, service_info);
}

// Runs on the UI service main thread.
// NOTE: For --mus the UI service is created at the //chrome/browser layer,
// not in //content. See ServiceManagerContext.
std::unique_ptr<service_manager::Service> CreateUiService(
    const scoped_refptr<base::SingleThreadTaskRunner>& resource_runner,
    base::WeakPtr<ui::ImageCursorsSet> image_cursors_set_weak_ptr) {
  ui::Service::InitParams params;
  params.running_standalone = false;
  params.resource_runner = resource_runner;
  params.image_cursors_set_weak_ptr = image_cursors_set_weak_ptr;
  params.should_host_viz = true;
  return std::make_unique<ui::Service>(params);
}

// Runs on the utility process main thread.
void RegisterUiService(
    content::ContentUtilityClient::StaticServiceMap* services,
    ui::ImageCursorsSet* cursors) {
  service_manager::EmbeddedServiceInfo service_info;
  service_info.use_own_thread = true;
  service_info.message_loop_type = base::MessageLoop::TYPE_UI;
  service_info.thread_priority = base::ThreadPriority::DISPLAY;
  service_info.factory =
      base::BindRepeating(&CreateUiService, base::ThreadTaskRunnerHandle::Get(),
                          cursors->GetWeakPtr());
  services->emplace(ui::mojom::kServiceName, service_info);
}

std::unique_ptr<service_manager::Service> CreateAshService() {
  const bool show_primary_host_on_connect = true;
  return std::make_unique<ash::WindowManagerService>(
      show_primary_host_on_connect);
}

std::unique_ptr<service_manager::Service> CreateAccessibilityAutoclick() {
  return std::make_unique<ash::autoclick::AutoclickApplication>();
}

std::unique_ptr<service_manager::Service> CreateQuickLaunch() {
  return std::make_unique<quick_launch::QuickLaunch>();
}

std::unique_ptr<service_manager::Service> CreateTouchHud() {
  return std::make_unique<ash::touch_hud::TouchHudApplication>();
}

std::unique_ptr<service_manager::Service> CreateFontService() {
  return std::make_unique<font_service::FontServiceApp>();
}

}  // namespace

MashServiceFactory::MashServiceFactory()
    : cursors_(std::make_unique<ui::ImageCursorsSet>()) {}

MashServiceFactory::~MashServiceFactory() = default;

void MashServiceFactory::RegisterOutOfProcessServices(
    content::ContentUtilityClient::StaticServiceMap* services) {
  RegisterUiService(services, cursors_.get());
  RegisterMashService(services, quick_launch::mojom::kServiceName,
                      &CreateQuickLaunch);
  RegisterMashService(services, ash::mojom::kServiceName, &CreateAshService);
  RegisterMashService(services, "accessibility_autoclick",
                      &CreateAccessibilityAutoclick);
  RegisterMashService(services, "touch_hud", &CreateTouchHud);
  RegisterMashService(services, font_service::mojom::kServiceName,
                      &CreateFontService);
}
