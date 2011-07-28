// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_LIVE_SYNC_BOOKMARKS_HELPER_H_
#define CHROME_TEST_LIVE_SYNC_BOOKMARKS_HELPER_H_
#pragma once

#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/test/live_sync/sync_datatype_helper.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

class GURL;
class Profile;

class BookmarksHelper : public SyncDatatypeHelper {
 public:
  // Used to access the bookmark model within a particular sync profile.
  static BookmarkModel* GetBookmarkModel(int index) WARN_UNUSED_RESULT;

  // Used to access the bookmark bar within a particular sync profile.
  static const BookmarkNode* GetBookmarkBarNode(int index) WARN_UNUSED_RESULT;

  // Used to access the "other bookmarks" node within a particular sync profile.
  static const BookmarkNode* GetOtherNode(int index) WARN_UNUSED_RESULT;

  // Used to access the bookmarks within the verifier sync profile.
  static BookmarkModel* GetVerifierBookmarkModel() WARN_UNUSED_RESULT;

  // Encrypt Bookmarks datatype.
  static bool EnableEncryption(int index);

  // Check if Bookmarks are encrypted.
  static bool IsEncrypted(int index);

  // Adds a URL with address |url| and title |title| to the bookmark bar of
  // profile |profile|. Returns a pointer to the node that was added.
  static const BookmarkNode* AddURL(
      int profile,
      const std::wstring& title,
      const GURL& url) WARN_UNUSED_RESULT;

  // Adds a URL with address |url| and title |title| to the bookmark bar of
  // profile |profile| at position |index|. Returns a pointer to the node that
  // was added.
  static const BookmarkNode* AddURL(
      int profile,
      int index,
      const std::wstring& title,
      const GURL& url) WARN_UNUSED_RESULT;

  // Adds a URL with address |url| and title |title| under the node |parent| of
  // profile |profile| at position |index|. Returns a pointer to the node that
  // was added.
  static const BookmarkNode* AddURL(
      int profile,
      const BookmarkNode* parent,
      int index,
      const std::wstring& title,
      const GURL& url) WARN_UNUSED_RESULT;

  // Adds a folder named |title| to the bookmark bar of profile |profile|.
  // Returns a pointer to the folder that was added.
  static const BookmarkNode* AddFolder(
      int profile,
      const std::wstring& title) WARN_UNUSED_RESULT;

  // Adds a folder named |title| to the bookmark bar of profile |profile| at
  // position |index|. Returns a pointer to the folder that was added.
  static const BookmarkNode* AddFolder(
      int profile,
      int index,
      const std::wstring& title) WARN_UNUSED_RESULT;

  // Adds a folder named |title| to the node |parent| in the bookmark model of
  // profile |profile| at position |index|. Returns a pointer to the node that
  // was added.
  static const BookmarkNode* AddFolder(
      int profile,
      const BookmarkNode* parent,
      int index,
      const std::wstring& title) WARN_UNUSED_RESULT;

  // Changes the title of the node |node| in the bookmark model of profile
  // |profile| to |new_title|.
  static void SetTitle(int profile,
                       const BookmarkNode* node,
                       const std::wstring& new_title);

  // Sets the favicon of the node |node| (of type BookmarkNode::URL) in the
  // bookmark model of profile |profile| using the data in |icon_bytes_vector|.
  static void SetFavicon(
      int profile,
      const BookmarkNode* node,
      const std::vector<unsigned char>& icon_bytes_vector);

  // Changes the url of the node |node| in the bookmark model of profile
  // |profile| to |new_url|. Returns a pointer to the node with the changed url.
  static const BookmarkNode* SetURL(
      int profile,
      const BookmarkNode* node,
      const GURL& new_url) WARN_UNUSED_RESULT;

  // Moves the node |node| in the bookmark model of profile |profile| so it ends
  // up under the node |new_parent| at position |index|.
  static void Move(
      int profile,
      const BookmarkNode* node,
      const BookmarkNode* new_parent,
      int index);

  // Removes the node in the bookmark model of profile |profile| under the node
  // |parent| at position |index|.
  static void Remove(int profile, const BookmarkNode* parent, int index);

  // Sorts the children of the node |parent| in the bookmark model of profile
  // |profile|.
  static void SortChildren(int profile, const BookmarkNode* parent);

  // Reverses the order of the children of the node |parent| in the bookmark
  // model of profile |profile|.
  static void ReverseChildOrder(int profile, const BookmarkNode* parent);

  // Checks if the bookmark model of profile |profile| matches the verifier
  // bookmark model. Returns true if they match.
  static bool ModelMatchesVerifier(int profile) WARN_UNUSED_RESULT;

  // Checks if the bookmark models of all sync profiles match the verifier
  // bookmark model. Returns true if they match.
  static bool AllModelsMatchVerifier() WARN_UNUSED_RESULT;

  // Checks if the bookmark models of |profile_a| and |profile_b| match each
  // other. Returns true if they match.
  static bool ModelsMatch(int profile_a, int profile_b) WARN_UNUSED_RESULT;

  // Checks if the bookmark models of all sync profiles match each other. Does
  // not compare them with the verifier bookmark model. Returns true if they
  // match.
  static bool AllModelsMatch() WARN_UNUSED_RESULT;

  // Checks if the bookmark model of profile |profile| contains any instances of
  // two bookmarks with the same URL under the same parent folder. Returns true
  // if even one instance is found.
  static bool ContainsDuplicateBookmarks(int profile);

  // Gets the node in the bookmark model of profile |profile| that has the url
  // |url|. Note: Only one instance of |url| is assumed to be present.
  static const BookmarkNode* GetUniqueNodeByURL(
      int profile,
      const GURL& url) WARN_UNUSED_RESULT;

  // Returns the number of bookmarks in bookmark model of profile |profile|
  // whose titles match the string |title|.
  static int CountBookmarksWithTitlesMatching(
      int profile,
      const std::wstring& title) WARN_UNUSED_RESULT;

  // Returns the number of bookmark folders in the bookmark model of profile
  // |profile| whose titles contain the query string |title|.
  static int CountFoldersWithTitlesMatching(
      int profile,
      const std::wstring& title) WARN_UNUSED_RESULT;

  // Creates a unique favicon using |seed|.
  static std::vector<unsigned char> CreateFavicon(int seed);

  // Returns a URL identifiable by |i|.
  static std::string IndexedURL(int i);

  // Returns a URL title identifiable by |i|.
  static std::wstring IndexedURLTitle(int i);

  // Returns a folder name identifiable by |i|.
  static std::wstring IndexedFolderName(int i);

  // Returns a subfolder name identifiable by |i|.
  static std::wstring IndexedSubfolderName(int i);

  // Returns a subsubfolder name identifiable by |i|.
  static std::wstring IndexedSubsubfolderName(int i);

 protected:
  BookmarksHelper();
  virtual ~BookmarksHelper();

 private:
  // Finds the node in the verifier bookmark model that corresponds to
  // |foreign_node| in |foreign_model| and stores its address in |result|.
  static void FindNodeInVerifier(BookmarkModel* foreign_model,
                                 const BookmarkNode* foreign_node,
                                 const BookmarkNode** result);

  // Returns the number of nodes of node type |node_type| in |model| whose
  // titles match the string |title|.
  static int CountNodesWithTitlesMatching(BookmarkModel* model,
                                          BookmarkNode::Type node_type,
                                          const string16& title);

  // Checks if the hierarchies in |model_a| and |model_b| are equivalent in
  // terms of the data model and favicon. Returns true if they both match.
  // Note: Some peripheral fields like creation times are allowed to mismatch.
  static bool BookmarkModelsMatch(BookmarkModel* model_a,
                                  BookmarkModel* model_b) WARN_UNUSED_RESULT;

  // Does a deep comparison of BookmarkNode fields in |model_a| and |model_b|.
  // Returns true if they are all equal.
  static bool NodesMatch(const BookmarkNode* model_a,
                         const BookmarkNode* model_b);

  // Checks if the favicon in |node_a| from |model_a| matches that of |node_b|
  // from |model_b|. Returns true if they match.
  static bool FaviconsMatch(BookmarkModel* model_a,
                            BookmarkModel* model_b,
                            const BookmarkNode* node_a,
                            const BookmarkNode* node_b);

  // Checks if the favicon data in |bitmap_a| and |bitmap_b| are equivalent.
  // Returns true if they match.
  static bool FaviconBitmapsMatch(const SkBitmap& bitmap_a,
                                  const SkBitmap& bitmap_b);

  // Gets the favicon associated with |node| in |model|.
  static const SkBitmap& GetFavicon(BookmarkModel* model,
                                    const BookmarkNode* node);

  // A collection of URLs for which we have added favicons. Since loading a
  // favicon is an asynchronous operation and doesn't necessarily invoke a
  // callback, this collection is used to determine if we must wait for a URL's
  // favicon to load or not.
  static std::set<GURL>* urls_with_favicons_;

  DISALLOW_COPY_AND_ASSIGN(BookmarksHelper);
};

#endif  // CHROME_TEST_LIVE_SYNC_BOOKMARKS_HELPER_H_
