// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UNDO_UNDO_MANAGER_UTILS_H_
#define CHROME_BROWSER_UNDO_UNDO_MANAGER_UTILS_H_

#include "base/basictypes.h"

class UndoManager;

// ScopedSuspendUndoTracking ---------------------------------------------------

// Scopes the suspension of the undo tracking for non-user initiated changes
// such as those occuring during profile synchronization.
class ScopedSuspendUndoTracking {
 public:
  explicit ScopedSuspendUndoTracking(UndoManager* undo_manager);
  ~ScopedSuspendUndoTracking();

 private:
  UndoManager* undo_manager_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSuspendUndoTracking);
};

// ScopedGroupingAction --------------------------------------------------------

// Scopes the grouping of a set of changes into one undoable action.
class ScopedGroupingAction {
 public:
  explicit ScopedGroupingAction(UndoManager* undo_manager);
  ~ScopedGroupingAction();

 private:
  UndoManager* undo_manager_;

  DISALLOW_COPY_AND_ASSIGN(ScopedGroupingAction);
};

#endif  // CHROME_BROWSER_UNDO_UNDO_MANAGER_UTILS_H_
