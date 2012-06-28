// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/bookmark_load_observer.h"

#include "chrome/test/base/ui_test_utils.h"

BookmarkLoadObserver::BookmarkLoadObserver(const base::Closure& quit_task)
    : quit_task_(quit_task) {}

BookmarkLoadObserver::~BookmarkLoadObserver() {}

void BookmarkLoadObserver::Loaded(BookmarkModel* model, bool ids_reassigned) {
  quit_task_.Run();
}
