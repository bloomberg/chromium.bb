// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UNDO_BOOKMARK_UNDO_UTILS_H_
#define CHROME_BROWSER_UNDO_BOOKMARK_UNDO_UTILS_H_

#include "base/basictypes.h"

class Profile;

// ScopedSuspendBookmarkUndo --------------------------------------------------

// Scopes the suspension of the undo tracking for non-user initiated changes
// such as those occuring during profile synchronization.
class ScopedSuspendBookmarkUndo {
 public:
  explicit ScopedSuspendBookmarkUndo(Profile* profile);
  ~ScopedSuspendBookmarkUndo();

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSuspendBookmarkUndo);
};

#endif  // CHROME_BROWSER_UNDO_BOOKMARK_UNDO_UTILS_H_
