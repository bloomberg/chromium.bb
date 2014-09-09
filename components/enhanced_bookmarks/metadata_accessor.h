// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENHANCED_BOOKMARKS_METADATA_ACCESSOR_H_
#define COMPONENTS_ENHANCED_BOOKMARKS_METADATA_ACCESSOR_H_

#include <set>
#include <string>
#include <vector>

class BookmarkModel;
class BookmarkNode;
class GURL;

// TODO(rfevang): Remove this file once the remaining caller
// is converted (enhanced_bookmarks_bridge.cc)

// The functions in this file store and retrieve structured data encoded in the
// bookmark metadata. This information suplements the data in the bookmark with
// images and descriptions related to the url.
namespace enhanced_bookmarks {

typedef std::vector<const BookmarkNode*> NodeVector;
typedef std::set<const BookmarkNode*> NodeSet;

// The keys used to store the data in the bookmarks metadata dictionary.
extern const char* kPageDataKey;
extern const char* kImageDataKey;
extern const char* kIdDataKey;
extern const char* kNoteKey;

// Returns the remoteId for a bookmark. If the bookmark doesn't have one already
// this function will create and set one.
std::string RemoteIdFromBookmark(BookmarkModel* bookmark_model,
                                 const BookmarkNode* node);

// Sets the description of a bookmark.
void SetDescriptionForBookmark(BookmarkModel* bookmark_model,
                               const BookmarkNode* node,
                               const std::string& description);

// Returns the description of a bookmark.
std::string DescriptionFromBookmark(const BookmarkNode* node);

// Sets the URL of an image representative of the page.
// Expects the URL to be valid and not empty.
// Returns true if the metainfo is successfully populated.
bool SetOriginalImageForBookmark(BookmarkModel* bookmark_model,
                                 const BookmarkNode* node,
                                 const GURL& url,
                                 int width,
                                 int height);

// Returns the url and dimensions of the original scraped image.
// Returns true if the out variables are populated, false otherwise.
bool OriginalImageFromBookmark(const BookmarkNode* node,
                               GURL* url,
                               int* width,
                               int* height);

// Returns the url and dimensions of the server provided thumbnail image.
// Returns true if the out variables are populated, false otherwise.
bool ThumbnailImageFromBookmark(const BookmarkNode* node,
                                GURL* url,
                                int* width,
                                int* height);

// Returns a brief server provided synopsis of the bookmarked page.
// Returns the empty string if the snippet could not be extracted.
std::string SnippetFromBookmark(const BookmarkNode* node);

// Used for testing, simulates the process that creates the thumnails. Will
// remove existing entries for empty urls or set them if the url is not empty.
// expects valid or empty urls. Returns true if the metainfo is successfully
// populated.
bool SetAllImagesForBookmark(BookmarkModel* bookmark_model,
                             const BookmarkNode* node,
                             const GURL& image_url,
                             int image_width,
                             int image_height,
                             const GURL& thumbnail_url,
                             int thumbnail_width,
                             int thumbnail_height);

}  // namespace enhanced_bookmarks

#endif  // COMPONENTS_ENHANCED_BOOKMARKS_METADATA_ACCESSOR_H_
