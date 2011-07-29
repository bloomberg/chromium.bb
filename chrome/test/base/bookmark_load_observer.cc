// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/bookmark_load_observer.h"

#include "base/message_loop.h"

BookmarkLoadObserver::BookmarkLoadObserver() {}

BookmarkLoadObserver::~BookmarkLoadObserver() {}

void BookmarkLoadObserver::Loaded(BookmarkModel* model, bool ids_reassigned) {
  MessageLoop::current()->Quit();
}
