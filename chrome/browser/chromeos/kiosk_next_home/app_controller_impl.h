// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_KIOSK_NEXT_HOME_APP_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_KIOSK_NEXT_HOME_APP_CONTROLLER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/chromeos/kiosk_next_home/mojom/app_controller.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"

class Profile;

namespace apps {
class AppServiceProxy;
}

namespace chromeos {
namespace kiosk_next_home {

// Implementation for the Kiosk Next AppController.
// This class is responsible for managing Chrome OS apps and returning useful
// information about them to the Kiosk Next Home.
class AppControllerImpl : public mojom::AppController {
 public:
  explicit AppControllerImpl(Profile* profile);
  ~AppControllerImpl() override;

  // Binds this implementation instance to an AppController request.
  void BindRequest(mojom::AppControllerRequest request);

  // mojom::AppController:
  void LaunchApp(const std::string& app_id) override;

 private:
  mojo::BindingSet<mojom::AppController> bindings_;
  apps::AppServiceProxy* app_service_proxy_;

  DISALLOW_COPY_AND_ASSIGN(AppControllerImpl);
};

}  // namespace kiosk_next_home
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_KIOSK_NEXT_HOME_APP_CONTROLLER_IMPL_H_
