// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENHANCED_BOOKMARKS_ENHANCED_BOOKMARK_MODEL_H_
#define COMPONENTS_ENHANCED_BOOKMARKS_ENHANCED_BOOKMARK_MODEL_H_

#include <map>
#include <string>

#include "base/cancelable_callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;

namespace base {
class Time;
}

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}

FORWARD_DECLARE_TEST(EnhancedBookmarkModelTest, SetMultipleMetaInfo);

namespace enhanced_bookmarks {

class EnhancedBookmarkModelObserver;

// Wrapper around BookmarkModel providing utility functions for enhanced
// bookmarks.
class EnhancedBookmarkModel : public KeyedService,
                              public bookmarks::BaseBookmarkModelObserver {
 public:
  EnhancedBookmarkModel(bookmarks::BookmarkModel* bookmark_model,
                        const std::string& version);
  ~EnhancedBookmarkModel() override;

  void Shutdown() override;

  void AddObserver(EnhancedBookmarkModelObserver* observer);
  void RemoveObserver(EnhancedBookmarkModelObserver* observer);

  // Moves |node| to |new_parent| and inserts it at the given |index|.
  void Move(const bookmarks::BookmarkNode* node,
            const bookmarks::BookmarkNode* new_parent,
            int index);

  // Adds a new folder node at the specified position.
  const bookmarks::BookmarkNode* AddFolder(
      const bookmarks::BookmarkNode* parent,
      int index,
      const base::string16& title);

  // Adds a url at the specified position.
  const bookmarks::BookmarkNode* AddURL(const bookmarks::BookmarkNode* parent,
                                        int index,
                                        const base::string16& title,
                                        const GURL& url,
                                        const base::Time& creation_time);

  // Returns the remote id for a bookmark |node|.
  std::string GetRemoteId(const bookmarks::BookmarkNode* node);

  // Returns the bookmark node corresponding to the given |remote_id|, or NULL
  // if there is no node with the id.
  const bookmarks::BookmarkNode* BookmarkForRemoteId(
      const std::string& remote_id);

  // Sets the description of a bookmark |node|.
  void SetDescription(const bookmarks::BookmarkNode* node,
                      const std::string& description);

  // Returns the description of a bookmark |node|.
  std::string GetDescription(const bookmarks::BookmarkNode* node);

  // Sets the URL of an image representative of a bookmark |node|.
  // Expects the URL to be valid and not empty.
  // Returns true if the metainfo is successfully populated.
  bool SetOriginalImage(const bookmarks::BookmarkNode* node,
                        const GURL& url,
                        int width,
                        int height);

  // Removes all image data for the node and sets the user_removed_image flag
  // so the server won't try to fetch a new image for the node.
  void RemoveImageData(const bookmarks::BookmarkNode* node);

  // Returns the url and dimensions of the original scraped image of a
  // bookmark |node|.
  // Returns true if the out variables are populated, false otherwise.
  bool GetOriginalImage(const bookmarks::BookmarkNode* node,
                        GURL* url,
                        int* width,
                        int* height);

  // Returns the url and dimensions of the server provided thumbnail image for
  // a given bookmark |node|.
  // Returns true if the out variables are populated, false otherwise.
  bool GetThumbnailImage(const bookmarks::BookmarkNode* node,
                         GURL* url,
                         int* width,
                         int* height);

  // Returns a brief server provided synopsis of the bookmarked page.
  // Returns the empty string if the snippet could not be extracted.
  std::string GetSnippet(const bookmarks::BookmarkNode* node);

  // Sets a custom suffix to be added to the version field when writing meta
  // info fields.
  void SetVersionSuffix(const std::string& version_suffix);

  // TODO(rfevang): Add method + enum for accessing/writing flags.

  // Used for testing, simulates the process that creates the thumbnails. Will
  // remove existing entries for empty urls or set them if the url is not empty.
  // Expects valid or empty urls. Returns true if the metainfo is successfully
  // populated.
  // TODO(rfevang): Move this to a testing only utility file.
  bool SetAllImages(const bookmarks::BookmarkNode* node,
                    const GURL& image_url,
                    int image_width,
                    int image_height,
                    const GURL& thumbnail_url,
                    int thumbnail_width,
                    int thumbnail_height);

  // TODO(rfevang): Ideally nothing should need the underlying bookmark model.
  // Remove when that is actually the case.
  bookmarks::BookmarkModel* bookmark_model() { return bookmark_model_; }

  // Returns true if the enhanced bookmark model is done loading.
  bool loaded() { return loaded_; }

  // Returns the version string to use when setting stars.version.
  std::string GetVersionString();

 private:
  FRIEND_TEST_ALL_PREFIXES(::EnhancedBookmarkModelTest, SetMultipleMetaInfo);

  typedef std::map<std::string, const bookmarks::BookmarkNode*> IdToNodeMap;
  typedef std::map<const bookmarks::BookmarkNode*, std::string> NodeToIdMap;

  // bookmarks::BaseBookmarkModelObserver:
  void BookmarkModelChanged() override;
  void BookmarkModelLoaded(bookmarks::BookmarkModel* model,
                           bool ids_reassigned) override;
  void BookmarkNodeAdded(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* parent,
                         int index) override;
  void BookmarkNodeRemoved(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* parent,
                           int old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& removed_urls) override;
  void BookmarkNodeChanged(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* node) override;
  void OnWillChangeBookmarkMetaInfo(
      bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* node) override;
  void BookmarkMetaInfoChanged(bookmarks::BookmarkModel* model,
                               const bookmarks::BookmarkNode* node) override;
  void BookmarkAllUserNodesRemoved(bookmarks::BookmarkModel* model,
                                   const std::set<GURL>& removed_urls) override;

  // Initialize the mapping from remote ids to nodes.
  void InitializeIdMap();

  // Adds a node to the id map if it has a (unique) remote id. Must be followed
  // by a (Schedule)ResetDuplicateRemoteIds call when done adding nodes.
  void AddToIdMap(const bookmarks::BookmarkNode* node);

  // Recursively removes a node and all its children from the various maps.
  void RemoveNodeFromMaps(const bookmarks::BookmarkNode* node);

  // If there are nodes that needs to reset their remote ids, schedules
  // ResetDuplicateRemoteIds to be run asynchronously.
  void ScheduleResetDuplicateRemoteIds();

  // Clears out any duplicate remote ids detected by AddToIdMap calls.
  void ResetDuplicateRemoteIds();

  // Helper method for setting a meta info field on a node. Also updates the
  // version field.
  void SetMetaInfo(const bookmarks::BookmarkNode* node,
                   const std::string& field,
                   const std::string& value);

  // Helper method for setting multiple meta info fields at once. All the fields
  // in |meta_info| will be set, but the method will not delete fields not
  // present.
  void SetMultipleMetaInfo(const bookmarks::BookmarkNode* node,
                           bookmarks::BookmarkNode::MetaInfoMap meta_info);

  bookmarks::BookmarkModel* bookmark_model_;
  bool loaded_;

  base::ObserverList<EnhancedBookmarkModelObserver> observers_;

  IdToNodeMap id_map_;
  NodeToIdMap nodes_to_reset_;

  // Caches the remote id of a node before its meta info changes.
  std::string prev_remote_id_;

  std::string version_;
  std::string version_suffix_;

  base::WeakPtrFactory<EnhancedBookmarkModel> weak_ptr_factory_;
};

}  // namespace enhanced_bookmarks

#endif  // COMPONENTS_ENHANCED_BOOKMARKS_ENHANCED_BOOKMARK_MODEL_H_
