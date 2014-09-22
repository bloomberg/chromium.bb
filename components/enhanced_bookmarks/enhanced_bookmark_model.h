// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENHANCED_BOOKMARKS_ENHANCED_BOOKMARK_MODEL_H_
#define COMPONENTS_ENHANCED_BOOKMARKS_ENHANCED_BOOKMARK_MODEL_H_

#include <map>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
class Time;
}  // namespace base

class BookmarkModel;
class BookmarkNode;
class GURL;

FORWARD_DECLARE_TEST(EnhancedBookmarkModelTest, SetMultipleMetaInfo);

namespace enhanced_bookmarks {

class EnhancedBookmarkModelObserver;

// Wrapper around BookmarkModel providing utility functions for enhanced
// bookmarks.
class EnhancedBookmarkModel : public KeyedService,
                              public BaseBookmarkModelObserver {
 public:
  EnhancedBookmarkModel(BookmarkModel* bookmark_model,
                        const std::string& version);
  virtual ~EnhancedBookmarkModel();

  virtual void Shutdown() OVERRIDE;

  void AddObserver(EnhancedBookmarkModelObserver* observer);
  void RemoveObserver(EnhancedBookmarkModelObserver* observer);

  // Moves |node| to |new_parent| and inserts it at the given |index|.
  void Move(const BookmarkNode* node,
            const BookmarkNode* new_parent,
            int index);

  // Adds a new folder node at the specified position.
  const BookmarkNode* AddFolder(const BookmarkNode* parent,
                                int index,
                                const base::string16& title);

  // Adds a url at the specified position.
  const BookmarkNode* AddURL(const BookmarkNode* parent,
                             int index,
                             const base::string16& title,
                             const GURL& url,
                             const base::Time& creation_time);

  // Returns the remote id for a bookmark |node|.
  std::string GetRemoteId(const BookmarkNode* node);

  // Returns the bookmark node corresponding to the given |remote_id|, or NULL
  // if there is no node with the id.
  const BookmarkNode* BookmarkForRemoteId(const std::string& remote_id);

  // Sets the description of a bookmark |node|.
  void SetDescription(const BookmarkNode* node, const std::string& description);

  // Returns the description of a bookmark |node|.
  std::string GetDescription(const BookmarkNode* node);

  // Sets the URL of an image representative of a bookmark |node|.
  // Expects the URL to be valid and not empty.
  // Returns true if the metainfo is successfully populated.
  bool SetOriginalImage(const BookmarkNode* node,
                        const GURL& url,
                        int width,
                        int height);

  // Returns the url and dimensions of the original scraped image of a
  // bookmark |node|.
  // Returns true if the out variables are populated, false otherwise.
  bool GetOriginalImage(const BookmarkNode* node,
                        GURL* url,
                        int* width,
                        int* height);

  // Returns the url and dimensions of the server provided thumbnail image for
  // a given bookmark |node|.
  // Returns true if the out variables are populated, false otherwise.
  bool GetThumbnailImage(const BookmarkNode* node,
                         GURL* url,
                         int* width,
                         int* height);

  // Returns a brief server provided synopsis of the bookmarked page.
  // Returns the empty string if the snippet could not be extracted.
  std::string GetSnippet(const BookmarkNode* node);

  // Sets a custom suffix to be added to the version field when writing meta
  // info fields.
  void SetVersionSuffix(const std::string& version_suffix);

  // TODO(rfevang): Add method + enum for accessing/writing flags.

  // Used for testing, simulates the process that creates the thumbnails. Will
  // remove existing entries for empty urls or set them if the url is not empty.
  // Expects valid or empty urls. Returns true if the metainfo is successfully
  // populated.
  // TODO(rfevang): Move this to a testing only utility file.
  bool SetAllImages(const BookmarkNode* node,
                    const GURL& image_url,
                    int image_width,
                    int image_height,
                    const GURL& thumbnail_url,
                    int thumbnail_width,
                    int thumbnail_height);

  // TODO(rfevang): Ideally nothing should need the underlying bookmark model.
  // Remove when that is actually the case.
  BookmarkModel* bookmark_model() { return bookmark_model_; }

  // Returns true if the enhanced bookmark model is done loading.
  bool loaded() { return loaded_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(::EnhancedBookmarkModelTest, SetMultipleMetaInfo);

  typedef std::map<std::string, const BookmarkNode*> IdToNodeMap;
  typedef std::map<const BookmarkNode*, std::string> NodeToIdMap;

  // BaseBookmarkModelObserver:
  virtual void BookmarkModelChanged() OVERRIDE;
  virtual void BookmarkModelLoaded(BookmarkModel* model,
                                   bool ids_reassigned) OVERRIDE;
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index) OVERRIDE;
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int old_index,
                                   const BookmarkNode* node,
                                   const std::set<GURL>& removed_urls) OVERRIDE;
  virtual void OnWillChangeBookmarkMetaInfo(BookmarkModel* model,
                                            const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkMetaInfoChanged(BookmarkModel* model,
                                       const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkAllUserNodesRemoved(
      BookmarkModel* model,
      const std::set<GURL>& removed_urls) OVERRIDE;

  // Initialize the mapping from remote ids to nodes.
  void InitializeIdMap();

  // Adds a node to the id map if it has a (unique) remote id. Must be followed
  // by a (Schedule)ResetDuplicateRemoteIds call when done adding nodes.
  void AddToIdMap(const BookmarkNode* node);

  // If there are nodes that needs to reset their remote ids, schedules
  // ResetDuplicateRemoteIds to be run asynchronously.
  void ScheduleResetDuplicateRemoteIds();

  // Clears out any duplicate remote ids detected by AddToIdMap calls.
  void ResetDuplicateRemoteIds();

  // Helper method for setting a meta info field on a node. Also updates the
  // version field.
  void SetMetaInfo(const BookmarkNode* node,
                   const std::string& field,
                   const std::string& value);

  // Helper method for setting multiple meta info fields at once. All the fields
  // in |meta_info| will be set, but the method will not delete fields not
  // present.
  void SetMultipleMetaInfo(const BookmarkNode* node,
                           BookmarkNode::MetaInfoMap meta_info);

  // Returns the version string to use when setting stars.version.
  std::string GetVersionString();

  BookmarkModel* bookmark_model_;
  bool loaded_;

  ObserverList<EnhancedBookmarkModelObserver> observers_;

  base::WeakPtrFactory<EnhancedBookmarkModel> weak_ptr_factory_;

  IdToNodeMap id_map_;
  NodeToIdMap nodes_to_reset_;

  // Caches the remote id of a node before its meta info changes.
  std::string prev_remote_id_;

  std::string version_;
  std::string version_suffix_;
};

}  // namespace enhanced_bookmarks

#endif  // COMPONENTS_ENHANCED_BOOKMARKS_ENHANCED_BOOKMARK_MODEL_H_
