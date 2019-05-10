// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SUPERVISION_ONBOARDING_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_SUPERVISION_ONBOARDING_CONTROLLER_IMPL_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/chromeos/supervision/mojom/onboarding_controller.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace chromeos {
namespace supervision {

class OnboardingControllerImpl : public mojom::OnboardingController {
 public:
  OnboardingControllerImpl();
  ~OnboardingControllerImpl() override;

  void BindRequest(mojom::OnboardingControllerRequest request);

 private:
  // mojom::OnboardingController:
  void BindWebviewHost(mojom::OnboardingWebviewHostPtr webview_host) override;
  void HandleAction(mojom::OnboardingFlowAction action) override;

  mojom::OnboardingWebviewHostPtr webview_host_;
  mojo::BindingSet<mojom::OnboardingController> bindings_;

  DISALLOW_COPY_AND_ASSIGN(OnboardingControllerImpl);
};

}  // namespace supervision
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SUPERVISION_ONBOARDING_CONTROLLER_IMPL_H_
