// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_KIOSK_NEXT_HOME_IDENTITY_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_KIOSK_NEXT_HOME_IDENTITY_CONTROLLER_IMPL_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/kiosk_next_home/mojom/identity_controller.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace chromeos {
namespace kiosk_next_home {

// IdentityController implementation to expose identity-related capabilities to
// Kiosk Next Home.
class IdentityControllerImpl : public mojom::IdentityController {
 public:
  explicit IdentityControllerImpl(mojom::IdentityControllerRequest request);
  ~IdentityControllerImpl() override;

  // mojom::IdentityController:
  void GetUserInfo(
      mojom::IdentityController::GetUserInfoCallback callback) override;

 private:
  mojo::Binding<mojom::IdentityController> binding_;

  DISALLOW_COPY_AND_ASSIGN(IdentityControllerImpl);
};

}  // namespace kiosk_next_home
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_KIOSK_NEXT_HOME_IDENTITY_CONTROLLER_IMPL_H_
