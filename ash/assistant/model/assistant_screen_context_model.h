// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_SCREEN_CONTEXT_MODEL_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_SCREEN_CONTEXT_MODEL_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace ash {

class AssistantScreenContextModelObserver;

// Enumeration of screen context request states.
enum class ScreenContextRequestState {
  kIdle,
  kInProgress,
};

class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantScreenContextModel {
 public:
  AssistantScreenContextModel();
  ~AssistantScreenContextModel();

  // Adds/removes the specified screen context model |observer|.
  void AddObserver(AssistantScreenContextModelObserver* observer);
  void RemoveObserver(AssistantScreenContextModelObserver* observer);

  // Sets the screen context request state.
  void SetRequestState(ScreenContextRequestState request_state);

 private:
  void NotifyRequestStateChanged();

  ScreenContextRequestState request_state_ = ScreenContextRequestState::kIdle;

  base::ObserverList<AssistantScreenContextModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AssistantScreenContextModel);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_SCREEN_CONTEXT_MODEL_H_
