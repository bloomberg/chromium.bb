// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_SETUP_CONTROLLER_H_
#define ASH_ASSISTANT_ASSISTANT_SETUP_CONTROLLER_H_

#include <map>
#include <string>

#include "ash/assistant/assistant_controller_observer.h"
#include "ash/assistant/ui/main_stage/assistant_opt_in_view.h"
#include "ash/public/interfaces/assistant_controller.mojom.h"
#include "ash/public/interfaces/assistant_setup.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace ash {

class AssistantController;

class AssistantSetupController : public mojom::AssistantSetupController,
                                 public AssistantControllerObserver,
                                 public AssistantOptInDelegate {
 public:
  explicit AssistantSetupController(AssistantController* assistant_controller);
  ~AssistantSetupController() override;

  void BindRequest(mojom::AssistantSetupControllerRequest request);

  // mojom::AssistantSetupController:
  void SetAssistantSetup(mojom::AssistantSetupPtr assistant_setup) override;

  // AssistantControllerObserver:
  void OnDeepLinkReceived(
      assistant::util::DeepLinkType type,
      const std::map<std::string, std::string>& params) override;

  // AssistantOptInDelegate:
  void OnOptInButtonPressed() override;

 private:
  void StartOnboarding(bool relaunch);

  AssistantController* const assistant_controller_;  // Owned by Shell.

  mojo::Binding<mojom::AssistantSetupController> binding_;

  // Interface to AssistantSetup in chrome/browser.
  mojom::AssistantSetupPtr assistant_setup_;

  DISALLOW_COPY_AND_ASSIGN(AssistantSetupController);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_SETUP_CONTROLLER_H_
