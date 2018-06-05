// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/assistant_bubble_model.h"

#include "ash/assistant/model/assistant_bubble_model_observer.h"

namespace ash {

AssistantBubbleModel::AssistantBubbleModel() = default;

AssistantBubbleModel::~AssistantBubbleModel() = default;

void AssistantBubbleModel::AddObserver(AssistantBubbleModelObserver* observer) {
  observers_.AddObserver(observer);
}

void AssistantBubbleModel::RemoveObserver(
    AssistantBubbleModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AssistantBubbleModel::SetUiMode(AssistantUiMode ui_mode) {
  if (ui_mode == ui_mode_)
    return;

  ui_mode_ = ui_mode;
  NotifyUiModeChanged();
}

void AssistantBubbleModel::NotifyUiModeChanged() {
  for (AssistantBubbleModelObserver& observer : observers_)
    observer.OnUiModeChanged(ui_mode_);
}

}  // namespace ash
