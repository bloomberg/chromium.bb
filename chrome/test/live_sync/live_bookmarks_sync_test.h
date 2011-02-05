// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_LIVE_SYNC_LIVE_BOOKMARKS_SYNC_TEST_H_
#define CHROME_TEST_LIVE_SYNC_LIVE_BOOKMARKS_SYNC_TEST_H_
#pragma once

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/live_sync/bookmark_model_verifier.h"
#include "chrome/test/live_sync/live_sync_test.h"
#include "chrome/test/ui_test_utils.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"

class LiveBookmarksSyncTest : public LiveSyncTest {
 public:
  explicit LiveBookmarksSyncTest(TestType test_type)
      : LiveSyncTest(test_type) {}

  virtual ~LiveBookmarksSyncTest() {}

  // Sets up sync profiles and clients and initializes the bookmark verifier.
  virtual bool SetupClients() WARN_UNUSED_RESULT {
    if (!LiveSyncTest::SetupClients())
      return false;
    for (int i = 0; i < num_clients(); ++i) {
      ui_test_utils::WaitForBookmarkModelToLoad(
          GetProfile(i)->GetBookmarkModel());
    }
    verifier_helper_.reset(
        new BookmarkModelVerifier(GetVerifierBookmarkModel()));
    ui_test_utils::WaitForBookmarkModelToLoad(verifier()->GetBookmarkModel());
    return true;
  }

  // Used to access the bookmark model within a particular sync profile.
  BookmarkModel* GetBookmarkModel(int index) WARN_UNUSED_RESULT {
    return GetProfile(index)->GetBookmarkModel();
  }

  // Used to access the bookmark bar within a particular sync profile.
  const BookmarkNode* GetBookmarkBarNode(int index) WARN_UNUSED_RESULT {
    return GetBookmarkModel(index)->GetBookmarkBarNode();
  }

  // Used to access the "other bookmarks" node within a particular sync profile.
  const BookmarkNode* GetOtherNode(int index) WARN_UNUSED_RESULT {
    return GetBookmarkModel(index)->other_node();
  }

  // Used to access the bookmarks within the verifier sync profile.
  BookmarkModel* GetVerifierBookmarkModel() WARN_UNUSED_RESULT {
    return verifier()->GetBookmarkModel();
  }

  // After calling this method, changes made to a bookmark model will no longer
  // be reflected in the verifier model.
  void DisableVerifier() { verifier_helper_->set_use_verifier_model(false); }

  // Adds a URL with address |url| and title |title| to the bookmark bar of
  // profile |profile|. Returns a pointer to the node that was added.
  const BookmarkNode* AddURL(int profile,
                             const std::wstring& title,
                             const GURL& url) WARN_UNUSED_RESULT {
    return verifier_helper_->AddURL(GetBookmarkModel(profile),
        GetBookmarkBarNode(profile), 0, WideToUTF16(title), url);
  }

  // Adds a URL with address |url| and title |title| to the bookmark bar of
  // profile |profile| at position |index|. Returns a pointer to the node that
  // was added.
  const BookmarkNode* AddURL(int profile,
                             int index,
                             const std::wstring& title,
                             const GURL& url) WARN_UNUSED_RESULT {
    return verifier_helper_->AddURL(GetBookmarkModel(profile),
        GetBookmarkBarNode(profile), index, WideToUTF16(title), url);
  }

  // Adds a URL with address |url| and title |title| under the node |parent| of
  // profile |profile| at position |index|. Returns a pointer to the node that
  // was added.
  const BookmarkNode* AddURL(int profile,
                             const BookmarkNode* parent,
                             int index,
                             const std::wstring& title,
                             const GURL& url) WARN_UNUSED_RESULT {
    EXPECT_EQ(GetBookmarkModel(profile)->GetNodeByID(parent->id()), parent);
    return verifier_helper_->AddURL(GetBookmarkModel(profile), parent, index,
        WideToUTF16(title), url);
  }

  // Adds a group named |title| to the bookmark bar of profile |profile|.
  // Returns a pointer to the group that was added.
  const BookmarkNode* AddGroup(int profile,
                               const std::wstring& title) WARN_UNUSED_RESULT {
    return verifier_helper_->AddGroup(GetBookmarkModel(profile),
        GetBookmarkBarNode(profile), 0, WideToUTF16(title));
  }

  // Adds a group named |title| to the bookmark bar of profile |profile| at
  // position |index|. Returns a pointer to the group that was added.
  const BookmarkNode* AddGroup(int profile,
                               int index,
                               const std::wstring& title) WARN_UNUSED_RESULT {
    return verifier_helper_->AddGroup(GetBookmarkModel(profile),
        GetBookmarkBarNode(profile), index, WideToUTF16(title));
  }

  // Adds a group named |title| to the node |parent| in the bookmark model of
  // profile |profile| at position |index|. Returns a pointer to the node that
  // was added.
  const BookmarkNode* AddGroup(int profile,
                               const BookmarkNode* parent,
                               int index,
                               const std::wstring& title) WARN_UNUSED_RESULT {
    if (GetBookmarkModel(profile)->GetNodeByID(parent->id()) != parent) {
      LOG(ERROR) << "Node " << parent->GetTitle() << " does not belong to "
                 << "Profile " << profile;
      return NULL;
    }
    return verifier_helper_->AddGroup(
        GetBookmarkModel(profile), parent, index, WideToUTF16(title));
  }

  // Changes the title of the node |node| in the bookmark model of profile
  // |profile| to |new_title|.
  void SetTitle(int profile,
                const BookmarkNode* node,
                const std::wstring& new_title) {
    ASSERT_EQ(GetBookmarkModel(profile)->GetNodeByID(node->id()), node)
        << "Node " << node->GetTitle() << " does not belong to "
        << "Profile " << profile;
    verifier_helper_->SetTitle(
        GetBookmarkModel(profile), node, WideToUTF16(new_title));
  }

  // Sets the favicon of the node |node| (of type BookmarkNode::URL) in the
  // bookmark model of profile |profile| using the data in |icon_bytes_vector|.
  void SetFavicon(int profile,
                  const BookmarkNode* node,
                  const std::vector<unsigned char>& icon_bytes_vector) {
    ASSERT_EQ(GetBookmarkModel(profile)->GetNodeByID(node->id()), node)
        << "Node " << node->GetTitle() << " does not belong to "
        << "Profile " << profile;
    ASSERT_EQ(BookmarkNode::URL, node->type())
        << "Node " << node->GetTitle() << " must be a url.";
    verifier_helper_->SetFavicon(
        GetBookmarkModel(profile), node, icon_bytes_vector);
  }

  // Changes the url of the node |node| in the bookmark model of profile
  // |profile| to |new_url|. Returns a pointer to the node with the changed url.
  const BookmarkNode* SetURL(int profile,
                             const BookmarkNode* node,
                             const GURL& new_url) WARN_UNUSED_RESULT {
    if (GetBookmarkModel(profile)->GetNodeByID(node->id()) != node) {
      LOG(ERROR) << "Node " << node->GetTitle() << " does not belong to "
                 << "Profile " << profile;
      return NULL;
    }
    return verifier_helper_->SetURL(GetBookmarkModel(profile), node, new_url);
  }

  // Moves the node |node| in the bookmark model of profile |profile| so it ends
  // up under the node |new_parent| at position |index|.
  void Move(int profile,
            const BookmarkNode* node,
            const BookmarkNode* new_parent,
            int index) {
    ASSERT_EQ(GetBookmarkModel(profile)->GetNodeByID(node->id()), node)
        << "Node " << node->GetTitle() << " does not belong to "
        << "Profile " << profile;
    verifier_helper_->Move(
        GetBookmarkModel(profile), node, new_parent, index);
  }

  // Removes the node in the bookmark model of profile |profile| under the node
  // |parent| at position |index|.
  void Remove(int profile, const BookmarkNode* parent, int index) {
    ASSERT_EQ(GetBookmarkModel(profile)->GetNodeByID(parent->id()), parent)
        << "Node " << parent->GetTitle() << " does not belong to "
        << "Profile " << profile;
    verifier_helper_->Remove(GetBookmarkModel(profile), parent, index);
  }

  // Sorts the children of the node |parent| in the bookmark model of profile
  // |profile|.
  void SortChildren(int profile, const BookmarkNode* parent) {
    ASSERT_EQ(GetBookmarkModel(profile)->GetNodeByID(parent->id()), parent)
        << "Node " << parent->GetTitle() << " does not belong to "
        << "Profile " << profile;
    verifier_helper_->SortChildren(GetBookmarkModel(profile), parent);
  }

  // Reverses the order of the children of the node |parent| in the bookmark
  // model of profile |profile|.
  void ReverseChildOrder(int profile, const BookmarkNode* parent) {
    ASSERT_EQ(GetBookmarkModel(profile)->GetNodeByID(parent->id()), parent)
        << "Node " << parent->GetTitle() << " does not belong to "
        << "Profile " << profile;
    verifier_helper_->ReverseChildOrder(GetBookmarkModel(profile), parent);
  }

  // Checks if the bookmark model of profile |profile| matches the verifier
  // bookmark model. Returns true if they match.
  bool ModelMatchesVerifier(int profile) WARN_UNUSED_RESULT {
    if (verifier_helper_->use_verifier_model() == false) {
      LOG(ERROR) << "Illegal to call ModelMatchesVerifier() after "
                 << "DisableVerifier(). Use ModelsMatch() instead.";
      return false;
    }
    return BookmarkModelVerifier::ModelsMatch(
        GetVerifierBookmarkModel(), GetBookmarkModel(profile));
  }

  // Checks if the bookmark models of all sync profiles match the verifier
  // bookmark model. Returns true if they match.
  bool AllModelsMatchVerifier() WARN_UNUSED_RESULT {
    if (verifier_helper_->use_verifier_model() == false) {
      LOG(ERROR) << "Illegal to call AllModelsMatchVerifier() after "
                 << "DisableVerifier(). Use AllModelsMatch() instead.";
      return false;
    }
    for (int i = 0; i < num_clients(); ++i) {
      if (!ModelMatchesVerifier(i)) {
        LOG(ERROR) << "Model " << i << " does not match the verifier.";
        return false;
      }
    }
    return true;
  }

  // Checks if the bookmark models of |profile_a| and |profile_b| match each
  // other. Returns true if they match.
  bool ModelsMatch(int profile_a, int profile_b) WARN_UNUSED_RESULT {
    return BookmarkModelVerifier::ModelsMatch(
        GetBookmarkModel(profile_a), GetBookmarkModel(profile_b));
  }

  // Checks if the bookmark models of all sync profiles match each other. Does
  // not compare them with the verifier bookmark model. Returns true if they
  // match.
  bool AllModelsMatch() WARN_UNUSED_RESULT {
    for (int i = 1; i < num_clients(); ++i) {
      if (!ModelsMatch(0, i)) {
        LOG(ERROR) << "Model " << i << " does not match Model 0.";
        return false;
      }
    }
    return true;
  }

  // Checks if the bookmark model of profile |profile| contains any instances of
  // two bookmarks with the same URL under the same parent folder. Returns true
  // if even one instance is found.
  bool ContainsDuplicateBookmarks(int profile) {
    return BookmarkModelVerifier::ContainsDuplicateBookmarks(
        GetBookmarkModel(profile));
  }

  // Gets the node in the bookmark model of profile |profile| that has the url
  // |url|. Note: Only one instance of |url| is assumed to be present.
  const BookmarkNode* GetUniqueNodeByURL(int profile,
                                         const GURL& url) WARN_UNUSED_RESULT {
    std::vector<const BookmarkNode*> nodes;
    GetBookmarkModel(profile)->GetNodesByURL(url, &nodes);
    EXPECT_EQ(1U, nodes.size());
    if (nodes.empty())
      return NULL;
    return nodes[0];
  }

  // Returns the number of bookmarks in bookmark model of profile |profile|
  // whose titles match the string |title|.
  int CountBookmarksWithTitlesMatching(int profile, const std::wstring& title)
      WARN_UNUSED_RESULT {
    return verifier_helper_->CountNodesWithTitlesMatching(
        GetBookmarkModel(profile), BookmarkNode::URL, WideToUTF16(title));
  }

  // Returns the number of bookmark folders in the bookmark model of profile
  // |profile| whose titles contain the query string |title|.
  int CountFoldersWithTitlesMatching(int profile,
       const std::wstring& title) WARN_UNUSED_RESULT {
    return verifier_helper_->CountNodesWithTitlesMatching(
        GetBookmarkModel(profile), BookmarkNode::FOLDER, WideToUTF16(title));
  }

  // Creates a unique favicon using |seed|.
  static std::vector<unsigned char> CreateFavicon(int seed) {
    const int w = 16;
    const int h = 16;
    SkBitmap bmp;
    bmp.setConfig(SkBitmap::kARGB_8888_Config, w, h);
    bmp.allocPixels();
    uint32_t* src_data = bmp.getAddr32(0, 0);
    for (int i = 0; i < w * h; ++i) {
      src_data[i] = SkPreMultiplyARGB((seed + i) % 255,
                                      (seed + i) % 250,
                                      (seed + i) % 245,
                                      (seed + i) % 240);
    }
    std::vector<unsigned char> favicon;
    gfx::PNGCodec::EncodeBGRASkBitmap(bmp, false, &favicon);
    return favicon;
  }

 private:
  // Helper object that has the functionality to verify changes made to the
  // bookmarks of individual profiles.
  scoped_ptr<BookmarkModelVerifier> verifier_helper_;

  DISALLOW_COPY_AND_ASSIGN(LiveBookmarksSyncTest);
};

class SingleClientLiveBookmarksSyncTest : public LiveBookmarksSyncTest {
 public:
  SingleClientLiveBookmarksSyncTest()
      : LiveBookmarksSyncTest(SINGLE_CLIENT) {}
  ~SingleClientLiveBookmarksSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientLiveBookmarksSyncTest);
};

class TwoClientLiveBookmarksSyncTest : public LiveBookmarksSyncTest {
 public:
  TwoClientLiveBookmarksSyncTest()
      : LiveBookmarksSyncTest(TWO_CLIENT) {}
  ~TwoClientLiveBookmarksSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientLiveBookmarksSyncTest);
};

class MultipleClientLiveBookmarksSyncTest : public LiveBookmarksSyncTest {
 public:
  MultipleClientLiveBookmarksSyncTest()
      : LiveBookmarksSyncTest(MULTIPLE_CLIENT) {}
  ~MultipleClientLiveBookmarksSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MultipleClientLiveBookmarksSyncTest);
};

class ManyClientLiveBookmarksSyncTest : public LiveBookmarksSyncTest {
 public:
  ManyClientLiveBookmarksSyncTest()
      : LiveBookmarksSyncTest(MANY_CLIENT) {}
  ~ManyClientLiveBookmarksSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ManyClientLiveBookmarksSyncTest);
};

#endif  // CHROME_TEST_LIVE_SYNC_LIVE_BOOKMARKS_SYNC_TEST_H_
