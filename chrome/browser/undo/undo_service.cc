// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/undo/undo_service.h"

#include "base/logging.h"

UndoService::UndoService(Profile* profile)
    : profile_(profile),
      group_actions_count_(0),
      undo_suspended_count_(0) {
}

UndoService::~UndoService() {
}

void UndoService::Undo() {
}

void UndoService::Redo() {
}

size_t UndoService::undo_count() const {
  return 0;
}

size_t UndoService::redo_count() const {
  return 0;
}

void UndoService::StartGroupingActions() {
  ++group_actions_count_;
}

void UndoService::EndGroupingActions() {
  DCHECK_GT(undo_suspended_count_, 0);
  --group_actions_count_;
}

void UndoService::SuspendUndoTracking() {
  ++undo_suspended_count_;
}

void UndoService::ResumeUndoTracking() {
  DCHECK_GT(undo_suspended_count_, 0);
  --undo_suspended_count_;
}
