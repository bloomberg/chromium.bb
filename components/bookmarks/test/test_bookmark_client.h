// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_TEST_TEST_BOOKMARK_CLIENT_H_
#define COMPONENTS_BOOKMARKS_TEST_TEST_BOOKMARK_CLIENT_H_

#include "base/memory/scoped_ptr.h"
#include "components/bookmarks/browser/bookmark_client.h"

class BookmarkModel;

namespace test {

class TestBookmarkClient : public BookmarkClient {
 public:
  TestBookmarkClient() {}
  virtual ~TestBookmarkClient() {}

  // Create a BookmarkModel using this object as its client. The returned
  // BookmarkModel* is owned by the caller.
  scoped_ptr<BookmarkModel> CreateModel(bool index_urls);

 private:
  // BookmarkClient:
  virtual bool IsPermanentNodeVisible(int node_type) OVERRIDE;
  virtual void RecordAction(const base::UserMetricsAction& action) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(TestBookmarkClient);
};

}  // namespace test

#endif  // COMPONENTS_BOOKMARKS_TEST_TEST_BOOKMARK_CLIENT_H_
