// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/core/test/test_bookmark_client.h"

#include "components/bookmarks/core/browser/bookmark_model.h"
#include "components/bookmarks/core/browser/bookmark_storage.h"

namespace test {

scoped_ptr<BookmarkModel> TestBookmarkClient::CreateModel(bool index_urls) {
  scoped_ptr<BookmarkModel> bookmark_model(new BookmarkModel(this, index_urls));
  bookmark_model->DoneLoading(bookmark_model->CreateLoadDetails(std::string()));
  return bookmark_model.Pass();
}

bool TestBookmarkClient::PreferTouchIcon() {
  return false;
}

void TestBookmarkClient::RecordAction(const base::UserMetricsAction& action) {
}

}  // namespace test
