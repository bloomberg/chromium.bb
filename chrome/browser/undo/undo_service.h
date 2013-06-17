// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UNDO_UNDO_SERVICE_H_
#define CHROME_BROWSER_UNDO_UNDO_SERVICE_H_

#include "base/basictypes.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"

class Profile;

// UndoService ----------------------------------------------------------------

// UndoService will provide undo/redo service for the Profile. UndoService
// will initially serve the BookmarkModel providing a mechanism to group
// bookmark operations into user actions that can be undone and redone.
class UndoService : public BrowserContextKeyedService {
 public:
  explicit UndoService(Profile* profile);
  virtual ~UndoService();

  // Undo the last action.
  void Undo();

  // Redo the last action that was undone.
  void Redo();

  // Returns the number of undo and redo operations that are avaiable.
  size_t undo_count() const;
  size_t redo_count() const;

  // Group multiple operations into one undoable action.
  void StartGroupingActions();
  void EndGroupingActions();

  // Suspend tracking of user actions for the purposes of undo.  If operations
  // are triggered while undo is suspended any undo or redo actions will be
  // lost. This is needed for non-user modifications such as profile
  // synchonization.
  void SuspendUndoTracking();
  void ResumeUndoTracking();

 private:
  Profile* profile_;

  // Supports grouping operations into a single undo action.
  int group_actions_count_;

  // Indicates how many times SuspendUndoTracking has been called without the
  // corresponding ResumeUndoTracking.  While non-zero any modifications will
  // not trigger undo information to be recorded.
  int undo_suspended_count_;

  DISALLOW_COPY_AND_ASSIGN(UndoService);
};

#endif  // CHROME_BROWSER_UNDO_UNDO_SERVICE_H_
