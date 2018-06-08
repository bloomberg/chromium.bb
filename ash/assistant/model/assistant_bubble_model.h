// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_BUBBLE_MODEL_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_BUBBLE_MODEL_H_

#include "base/macros.h"
#include "base/observer_list.h"

namespace ash {

class AssistantBubbleModelObserver;

// Enumeration of Assistant UI modes.
enum class AssistantUiMode {
  kMainUi,
  kMiniUi,
  kWebUi,
};

// Models the Assistant bubble.
class AssistantBubbleModel {
 public:
  AssistantBubbleModel();
  ~AssistantBubbleModel();

  // Adds/removes the specified |observer|.
  void AddObserver(AssistantBubbleModelObserver* observer);
  void RemoveObserver(AssistantBubbleModelObserver* observer);

  // Sets the UI mode.
  void SetUiMode(AssistantUiMode ui_mode);

  // Returns the UI mode.
  AssistantUiMode ui_mode() const { return ui_mode_; }

 private:
  void NotifyUiModeChanged();

  AssistantUiMode ui_mode_ = AssistantUiMode::kMainUi;

  base::ObserverList<AssistantBubbleModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AssistantBubbleModel);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_BUBBLE_MODEL_H_
