// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_INTERNAL_CHROME_BROWSER_SHARE_EXTENSION_SHARE_EXTENSION_SERVICE_H_
#define IOS_INTERNAL_CHROME_BROWSER_SHARE_EXTENSION_SHARE_EXTENSION_SERVICE_H_

#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/reading_list/reading_list_model_observer.h"

namespace bookmarks {
class BookmarkModel;
}

namespace ios {
class ChromeBrowserState;
}

class ReadingListModel;

// AuthenticationService is the Chrome interface to the iOS shared
// authentication library.
class ShareExtensionService : public KeyedService,
                              public bookmarks::BaseBookmarkModelObserver,
                              public ReadingListModelObserver {
 public:
  explicit ShareExtensionService(bookmarks::BookmarkModel* bookmark_model,
                                 ReadingListModel* reading_list_model);
  ~ShareExtensionService() override;

  void Initialize();
  void Shutdown() override;
  void ReadingListModelLoaded(const ReadingListModel* model) override;
  void BookmarkModelLoaded(bookmarks::BookmarkModel* model,
                           bool ids_reassigned) override;

  void BookmarkModelChanged() override {}

 private:
  void AnyModelLoaded();

  ReadingListModel* reading_list_model_;  // not owned.
  bool reading_list_model_loaded_;
  bookmarks::BookmarkModel* bookmark_model_;  // not owned;
  bool bookmark_model_loaded_;

  DISALLOW_COPY_AND_ASSIGN(ShareExtensionService);
};

#endif  // IOS_INTERNAL_CHROME_BROWSER_SHARE_EXTENSION_SHARE_EXTENSION_SERVICE_H_
