// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_UI_MODEL_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_UI_MODEL_H_

#include "base/macros.h"
#include "base/observer_list.h"

namespace ash {

class AssistantUiModelObserver;

// Enumeration of Assistant entry/exit points.
enum class AssistantSource {
  kUnspecified,
  kDeepLink,
  kHotkey,
  kHotword,
  kLongPressLauncher,
  kSetup,
  kStylus,
  kLauncherSearchBox,
};

// Enumeration of Assistant UI modes.
enum class AssistantUiMode {
  kMainUi,
  kMiniUi,
  kWebUi,
};

// Models the Assistant UI.
class AssistantUiModel {
 public:
  AssistantUiModel();
  ~AssistantUiModel();

  // Adds/removes the specified |observer|.
  void AddObserver(AssistantUiModelObserver* observer);
  void RemoveObserver(AssistantUiModelObserver* observer);

  // Sets the UI mode.
  void SetUiMode(AssistantUiMode ui_mode);

  // Returns the UI mode.
  AssistantUiMode ui_mode() const { return ui_mode_; }

  // Sets the UI visibility.
  void SetVisible(bool visible, AssistantSource source);

  bool visible() const { return visible_; }

 private:
  void NotifyUiModeChanged();
  void NotifyUiVisibilityChanged(AssistantSource source);

  AssistantUiMode ui_mode_ = AssistantUiMode::kMainUi;

  bool visible_ = false;

  base::ObserverList<AssistantUiModelObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(AssistantUiModel);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_UI_MODEL_H_
