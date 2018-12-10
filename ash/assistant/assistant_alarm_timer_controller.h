// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_ALARM_TIMER_CONTROLLER_H_
#define ASH_ASSISTANT_ASSISTANT_ALARM_TIMER_CONTROLLER_H_

#include "ash/public/interfaces/assistant_controller.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace ash {

class AssistantController;

// The AssistantAlarmTimerController is a sub-controller of AssistantController
// tasked with tracking alarm/timer state and providing alarm/timer APIs.
class AssistantAlarmTimerController
    : public mojom::AssistantAlarmTimerController {
 public:
  explicit AssistantAlarmTimerController(
      AssistantController* assistant_controller);
  ~AssistantAlarmTimerController() override;

  void BindRequest(mojom::AssistantAlarmTimerControllerRequest request);

  // mojom::AssistantAlarmTimerController:
  void OnTimerSoundingStarted() override;
  void OnTimerSoundingFinished() override;

 private:
  AssistantController* const assistant_controller_;  // Owned by Shell.

  mojo::Binding<mojom::AssistantAlarmTimerController> binding_;

  DISALLOW_COPY_AND_ASSIGN(AssistantAlarmTimerController);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_ALARM_TIMER_CONTROLLER_H_
