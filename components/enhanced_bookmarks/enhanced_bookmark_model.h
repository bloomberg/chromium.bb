// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENHANCED_BOOKMARKS_ENHANCED_BOOKMARK_MODEL_H_
#define COMPONENTS_ENHANCED_BOOKMARKS_ENHANCED_BOOKMARK_MODEL_H_

#include <string>

#include "base/strings/string16.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
class Time;
}  // namespace base

class BookmarkModel;
class GURL;

namespace enhanced_bookmarks {
// Wrapper around BookmarkModel providing utility functions for enhanced
// bookmarks.
class EnhancedBookmarkModel : public KeyedService {
 public:
  EnhancedBookmarkModel(BookmarkModel* bookmark_model,
                        const std::string& version);
  virtual ~EnhancedBookmarkModel();

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

 private:
  // Generates and sets a remote id for the given bookmark |node|.
  // Returns the id set.
  std::string SetRemoteId(const BookmarkNode* node);

  // Helper method for setting a meta info field on a node. Also updates the
  // version and userEdits fields.
  void SetMetaInfo(const BookmarkNode* node,
                   const std::string& field,
                   const std::string& value,
                   bool user_edit);

  // Returns the version string to use when setting stars.version.
  std::string GetVersionString();

  BookmarkModel* bookmark_model_;
  std::string version_;
  std::string version_suffix_;
};

}  // namespace enhanced_bookmarks

#endif  // COMPONENTS_ENHANCED_BOOKMARKS_ENHANCED_BOOKMARK_MODEL_H_
