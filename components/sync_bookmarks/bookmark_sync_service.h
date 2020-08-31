// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_SYNC_SERVICE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/keyed_service/core/keyed_service.h"

class BookmarkUndoService;

namespace syncer {
class ModelTypeControllerDelegate;
}

namespace bookmarks {
class BookmarkModel;
}

namespace favicon {
class FaviconService;
}

namespace sync_bookmarks {
class BookmarkModelTypeProcessor;

// This service owns the BookmarkModelTypeProcessor.
class BookmarkSyncService : public KeyedService {
 public:
  // |bookmark_undo_service| must not be null and must outlive this object.
  explicit BookmarkSyncService(BookmarkUndoService* bookmark_undo_service);

  // KeyedService implemenation.
  ~BookmarkSyncService() override;

  // Analgous to Encode/Decode methods in BookmarkClient.
  std::string EncodeBookmarkSyncMetadata();
  void DecodeBookmarkSyncMetadata(
      const std::string& metadata_str,
      const base::RepeatingClosure& schedule_save_closure,
      bookmarks::BookmarkModel* model);

  // Returns the ModelTypeControllerDelegate for syncer::BOOKMARKS.
  // |favicon_service| is the favicon service used when processing updates in
  // the underlying processor. It could have been a separate a setter in
  // BookmarkSyncService instead of passing it as a parameter to
  // GetBookmarkSyncControllerDelegate(). However, this would incur the risk of
  // overlooking setting it. Therefore, it has been added as a parameter to the
  // GetBookmarkSyncControllerDelegate() in order to gauarantee it will be set
  // before the processor starts receiving updates.
  virtual base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetBookmarkSyncControllerDelegate(favicon::FaviconService* favicon_service);

 private:
  // BookmarkModelTypeProcessor handles communications between sync engine and
  // BookmarkModel/HistoryService.
  std::unique_ptr<sync_bookmarks::BookmarkModelTypeProcessor>
      bookmark_model_type_processor_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkSyncService);
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_SYNC_SERVICE_H_
