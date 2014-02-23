// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/undo/bookmark_undo_utils.h"

#include "chrome/browser/undo/bookmark_undo_service.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "chrome/browser/undo/undo_manager.h"

namespace {

// Utility funciton to safely return an UndoManager if available.
UndoManager* GetUndoManager(Profile* profile) {
  BookmarkUndoService* undo_service = profile ?
      BookmarkUndoServiceFactory::GetForProfile(profile) : NULL;
  return undo_service ? undo_service->undo_manager() : NULL;
}

}  // namespace

// ScopedSuspendBookmarkUndo --------------------------------------------------

ScopedSuspendBookmarkUndo::ScopedSuspendBookmarkUndo(Profile* profile)
    : profile_(profile) {
  UndoManager* undo_manager = GetUndoManager(profile_);
  if (undo_manager)
    undo_manager->SuspendUndoTracking();
}

ScopedSuspendBookmarkUndo::~ScopedSuspendBookmarkUndo() {
  UndoManager *undo_manager = GetUndoManager(profile_);
  if (undo_manager)
    undo_manager->ResumeUndoTracking();
}
