// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_ALARM_TIMER_CONTROLLER_H_
#define ASH_ASSISTANT_ASSISTANT_ALARM_TIMER_CONTROLLER_H_

#include "ash/assistant/model/assistant_alarm_timer_model.h"
#include "ash/assistant/model/assistant_alarm_timer_model_observer.h"
#include "ash/public/interfaces/assistant_controller.mojom.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace ash {

class AssistantController;

// The AssistantAlarmTimerController is a sub-controller of AssistantController
// tasked with tracking alarm/timer state and providing alarm/timer APIs.
class AssistantAlarmTimerController
    : public mojom::AssistantAlarmTimerController,
      public AssistantAlarmTimerModelObserver {
 public:
  explicit AssistantAlarmTimerController(
      AssistantController* assistant_controller);
  ~AssistantAlarmTimerController() override;

  void BindRequest(mojom::AssistantAlarmTimerControllerRequest request);

  // Returns the underlying model.
  const AssistantAlarmTimerModel* model() const { return &model_; }

  // Adds/removes the specified model |observer|.
  void AddModelObserver(AssistantAlarmTimerModelObserver* observer);
  void RemoveModelObserver(AssistantAlarmTimerModelObserver* observer);

  // mojom::AssistantAlarmTimerController:
  void OnTimerSoundingStarted() override;
  void OnTimerSoundingFinished() override;

  // AssistantAlarmTimerModelObserver:
  void OnAlarmTimerAdded(const AlarmTimer& alarm_timer,
                         const base::TimeDelta& time_remaining) override;
  void OnAlarmsTimersTicked(
      const std::map<std::string, base::TimeDelta>& times_remaining) override;
  void OnAllAlarmsTimersRemoved() override;

 private:
  AssistantController* const assistant_controller_;  // Owned by Shell.

  mojo::Binding<mojom::AssistantAlarmTimerController> binding_;

  AssistantAlarmTimerModel model_;

  base::RepeatingTimer timer_;

  int next_timer_id_ = 1;

  DISALLOW_COPY_AND_ASSIGN(AssistantAlarmTimerController);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_ALARM_TIMER_CONTROLLER_H_
