// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_SERVICE_H_
#define CHROME_BROWSER_CHROME_SERVICE_H_

#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/launchable.h"
#if defined(USE_OZONE)
#include "services/ui/public/cpp/input_devices/input_device_controller.h"
#endif
#endif

class ChromeService : public service_manager::Service {
 public:
  ChromeService();
  ~ChromeService() override;

  static std::unique_ptr<service_manager::Service> Create();

 private:
  // service_manager::Service:
  void OnBindInterface(const service_manager::BindSourceInfo& remote_info,
                       const std::string& name,
                       mojo::ScopedMessagePipeHandle handle) override;

  service_manager::BinderRegistry registry_;
  service_manager::BinderRegistryWithArgs<
      const service_manager::BindSourceInfo&>
      registry_with_source_info_;

#if defined(OS_CHROMEOS)
  chromeos::Launchable launchable_;
#if defined(USE_OZONE)
  ui::InputDeviceController input_device_controller_;
#endif
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromeService);
};

#endif  // CHROME_BROWSER_CHROME_SERVICE_H_
