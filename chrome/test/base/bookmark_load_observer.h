// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_BOOKMARK_LOAD_OBSERVER_H_
#define CHROME_TEST_BASE_BOOKMARK_LOAD_OBSERVER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "chrome/browser/bookmarks/base_bookmark_model_observer.h"

// BookmarkLoadObserver is used when blocking until the BookmarkModel finishes
// loading. As soon as the BookmarkModel finishes loading the message loop is
// quit.
class BookmarkLoadObserver : public BaseBookmarkModelObserver {
 public:
  explicit BookmarkLoadObserver(const base::Closure& quit_task);
  virtual ~BookmarkLoadObserver();

 private:
  // BaseBookmarkModelObserver:
  virtual void BookmarkModelChanged() OVERRIDE;
  virtual void Loaded(BookmarkModel* model, bool ids_reassigned) OVERRIDE;

  base::Closure quit_task_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkLoadObserver);
};

#endif  // CHROME_TEST_BASE_BOOKMARK_LOAD_OBSERVER_H_
