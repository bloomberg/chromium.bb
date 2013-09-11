// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_test_helpers.h"

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "chrome/browser/bookmarks/base_bookmark_model_observer.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "content/public/test/test_utils.h"

namespace {

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

BookmarkLoadObserver::BookmarkLoadObserver(const base::Closure& quit_task)
    : quit_task_(quit_task) {}

BookmarkLoadObserver::~BookmarkLoadObserver() {}

void BookmarkLoadObserver::BookmarkModelChanged() {}

void BookmarkLoadObserver::Loaded(BookmarkModel* model, bool ids_reassigned) {
  quit_task_.Run();
}

}  // namespace

namespace test {

void WaitForBookmarkModelToLoad(BookmarkModel* model) {
  if (model->loaded())
    return;
  base::RunLoop run_loop;
  BookmarkLoadObserver observer(content::GetQuitTaskForRunLoop(&run_loop));
  model->AddObserver(&observer);
  content::RunThisRunLoop(&run_loop);
  model->RemoveObserver(&observer);
  DCHECK(model->loaded());
}

void WaitForBookmarkModelToLoad(Profile* profile) {
  WaitForBookmarkModelToLoad(BookmarkModelFactory::GetForProfile(profile));
}

}  // namespace test
