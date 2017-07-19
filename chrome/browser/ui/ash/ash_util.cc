// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ash_util.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/mojo_interface_factory.h"
#include "ash/public/cpp/config.h"
#include "ash/public/interfaces/event_properties.mojom.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/chromeos/ash_config.h"
#include "chrome/browser/ui/ash/ash_init.h"
#include "content/public/common/service_names.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/interfaces/interface_provider_spec.mojom.h"
#include "ui/aura/window_event_dispatcher.h"

namespace ash_util {

namespace {

class EmbeddedAshService : public service_manager::Service {
 public:
  explicit EmbeddedAshService(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
      : task_runner_(task_runner) {}
  ~EmbeddedAshService() override {}

  // service_manager::Service:
  void OnStart() override {
    ash::mojo_interface_factory::RegisterInterfaces(&interfaces_, task_runner_);
  }

  void OnBindInterface(const service_manager::BindSourceInfo& remote_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle handle) override {
    interfaces_.BindInterface(interface_name, std::move(handle));
  }

 private:
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  service_manager::BinderRegistry interfaces_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedAshService);
};

}  // namespace

std::unique_ptr<service_manager::Service> CreateEmbeddedAshService(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  return base::MakeUnique<EmbeddedAshService>(task_runner);
}

bool ShouldOpenAshOnStartup() {
  return !IsRunningInMash();
}

bool IsRunningInMash() {
  return chromeos::GetAshConfig() == ash::Config::MASH;
}

bool IsAcceleratorDeprecated(const ui::Accelerator& accelerator) {
  // When running in mash the browser doesn't handle ash accelerators.
  if (IsRunningInMash())
    return false;

  return ash::Shell::Get()->accelerator_controller()->IsDeprecated(accelerator);
}

bool WillAshProcessAcceleratorForEvent(const ui::KeyEvent& key_event) {
  return key_event.properties() &&
         key_event.properties()->count(
             ash::mojom::kWillProcessAccelerator_KeyEventProperty);
}

}  // namespace ash_util
