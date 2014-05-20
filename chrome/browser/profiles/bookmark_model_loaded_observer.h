// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_BOOKMARK_MODEL_LOADED_OBSERVER_H_
#define CHROME_BROWSER_PROFILES_BOOKMARK_MODEL_LOADED_OBSERVER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"

class Profile;

class BookmarkModelLoadedObserver : public BaseBookmarkModelObserver {
 public:
  explicit BookmarkModelLoadedObserver(Profile* profile);

 private:
  virtual void BookmarkModelChanged() OVERRIDE;
  virtual void BookmarkModelLoaded(BookmarkModel* model,
                                   bool ids_reassigned) OVERRIDE;
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model) OVERRIDE;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkModelLoadedObserver);
};

#endif  // CHROME_BROWSER_PROFILES_BOOKMARK_MODEL_LOADED_OBSERVER_H_
