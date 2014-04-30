// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/core/browser/scoped_group_bookmark_actions.h"

#include "components/bookmarks/core/browser/bookmark_model.h"

ScopedGroupBookmarkActions::ScopedGroupBookmarkActions(BookmarkModel* model)
    : model_(model) {
  if (model_)
    model_->BeginGroupedChanges();
}

ScopedGroupBookmarkActions::~ScopedGroupBookmarkActions() {
  if (model_)
    model_->EndGroupedChanges();
}
