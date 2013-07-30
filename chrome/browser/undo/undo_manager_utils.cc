// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/undo/undo_manager_utils.h"

#include "chrome/browser/undo/undo_manager.h"

// ScopedSuspendUndoTracking --------------------------------------------------

ScopedSuspendUndoTracking::ScopedSuspendUndoTracking(UndoManager* undo_manager)
    : undo_manager_(undo_manager) {
  undo_manager_->SuspendUndoTracking();
}

ScopedSuspendUndoTracking::~ScopedSuspendUndoTracking() {
  undo_manager_->ResumeUndoTracking();
}

// ScopedGroupingAction -------------------------------------------------------

ScopedGroupingAction::ScopedGroupingAction(UndoManager* undo_manager)
    : undo_manager_(undo_manager) {
  undo_manager_->StartGroupingActions();
}

ScopedGroupingAction::~ScopedGroupingAction() {
  undo_manager_->EndGroupingActions();
}
