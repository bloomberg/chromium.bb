// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/scoped_group_bookmark_actions.h"

#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"

ScopedGroupBookmarkActions::ScopedGroupBookmarkActions(Profile* profile)
    : model_(NULL) {
  if (profile)
    model_ = BookmarkModelFactory::GetForProfile(profile);
  if (model_)
    model_->BeginGroupedChanges();
}

ScopedGroupBookmarkActions::ScopedGroupBookmarkActions(BookmarkModel* model)
    : model_(model) {
  if (model_)
    model_->BeginGroupedChanges();
}

ScopedGroupBookmarkActions::~ScopedGroupBookmarkActions() {
  if (model_)
    model_->EndGroupedChanges();
}
