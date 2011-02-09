// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/live_sync/live_bookmarks_sync_test.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/ui_test_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"

LiveBookmarksSyncTest::LiveBookmarksSyncTest(TestType test_type)
    : LiveSyncTest(test_type) {}

LiveBookmarksSyncTest::~LiveBookmarksSyncTest() {}

bool LiveBookmarksSyncTest::SetupClients() {
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

BookmarkModel* LiveBookmarksSyncTest::GetBookmarkModel(int index) {
  return GetProfile(index)->GetBookmarkModel();
}

const BookmarkNode* LiveBookmarksSyncTest::GetBookmarkBarNode(int index) {
  return GetBookmarkModel(index)->GetBookmarkBarNode();
}

const BookmarkNode* LiveBookmarksSyncTest::GetOtherNode(int index) {
  return GetBookmarkModel(index)->other_node();
}

BookmarkModel* LiveBookmarksSyncTest::GetVerifierBookmarkModel() {
  return verifier()->GetBookmarkModel();
}

void LiveBookmarksSyncTest::DisableVerifier() {
  verifier_helper_->set_use_verifier_model(false);
}

const BookmarkNode* LiveBookmarksSyncTest::AddURL(int profile,
                                                  const std::wstring& title,
                                                  const GURL& url) {
  return verifier_helper_->AddURL(GetBookmarkModel(profile),
                                  GetBookmarkBarNode(profile),
                                  0, WideToUTF16(title), url);
}

const BookmarkNode* LiveBookmarksSyncTest::AddURL(int profile,
                                                  int index,
                                                  const std::wstring& title,
                                                  const GURL& url) {
  return verifier_helper_->AddURL(GetBookmarkModel(profile),
                                  GetBookmarkBarNode(profile),
                                  index, WideToUTF16(title), url);
}

const BookmarkNode* LiveBookmarksSyncTest::AddURL(int profile,
                                                  const BookmarkNode* parent,
                                                  int index,
                                                  const std::wstring& title,
                                                  const GURL& url) {
  EXPECT_EQ(GetBookmarkModel(profile)->GetNodeByID(parent->id()), parent);
  return verifier_helper_->AddURL(GetBookmarkModel(profile), parent, index,
                                  WideToUTF16(title), url);
}

const BookmarkNode* LiveBookmarksSyncTest::AddGroup(int profile,
                                                    const std::wstring& title) {
  return verifier_helper_->AddGroup(GetBookmarkModel(profile),
                                    GetBookmarkBarNode(profile),
                                    0, WideToUTF16(title));
}

const BookmarkNode* LiveBookmarksSyncTest::AddGroup(int profile,
                                                    int index,
                                                    const std::wstring& title) {
  return verifier_helper_->AddGroup(GetBookmarkModel(profile),
                                    GetBookmarkBarNode(profile),
                                    index, WideToUTF16(title));
}

const BookmarkNode* LiveBookmarksSyncTest::AddGroup(int profile,
                                                    const BookmarkNode* parent,
                                                    int index,
                                                    const std::wstring& title) {
  if (GetBookmarkModel(profile)->GetNodeByID(parent->id()) != parent) {
    LOG(ERROR) << "Node " << parent->GetTitle() << " does not belong to "
               << "Profile " << profile;
    return NULL;
  }
  return verifier_helper_->AddGroup(
      GetBookmarkModel(profile), parent, index, WideToUTF16(title));
}

void LiveBookmarksSyncTest::SetTitle(int profile,
                                     const BookmarkNode* node,
                                     const std::wstring& new_title) {
  ASSERT_EQ(GetBookmarkModel(profile)->GetNodeByID(node->id()), node)
      << "Node " << node->GetTitle() << " does not belong to "
      << "Profile " << profile;
  verifier_helper_->SetTitle(
      GetBookmarkModel(profile), node, WideToUTF16(new_title));
}

void LiveBookmarksSyncTest::SetFavicon(
    int profile,
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

const BookmarkNode* LiveBookmarksSyncTest::SetURL(int profile,
                                                  const BookmarkNode* node,
                                                  const GURL& new_url) {
  if (GetBookmarkModel(profile)->GetNodeByID(node->id()) != node) {
    LOG(ERROR) << "Node " << node->GetTitle() << " does not belong to "
               << "Profile " << profile;
    return NULL;
  }
  return verifier_helper_->SetURL(GetBookmarkModel(profile), node, new_url);
}

void LiveBookmarksSyncTest::Move(int profile,
                                 const BookmarkNode* node,
                                 const BookmarkNode* new_parent,
                                 int index) {
  ASSERT_EQ(GetBookmarkModel(profile)->GetNodeByID(node->id()), node)
      << "Node " << node->GetTitle() << " does not belong to "
      << "Profile " << profile;
  verifier_helper_->Move(
      GetBookmarkModel(profile), node, new_parent, index);
}


void LiveBookmarksSyncTest::Remove(int profile, const BookmarkNode* parent,
                                   int index) {
  ASSERT_EQ(GetBookmarkModel(profile)->GetNodeByID(parent->id()), parent)
      << "Node " << parent->GetTitle() << " does not belong to "
      << "Profile " << profile;
  verifier_helper_->Remove(GetBookmarkModel(profile), parent, index);
}

void LiveBookmarksSyncTest::SortChildren(int profile,
                                         const BookmarkNode* parent) {
  ASSERT_EQ(GetBookmarkModel(profile)->GetNodeByID(parent->id()), parent)
      << "Node " << parent->GetTitle() << " does not belong to "
      << "Profile " << profile;
  verifier_helper_->SortChildren(GetBookmarkModel(profile), parent);
}

void LiveBookmarksSyncTest::ReverseChildOrder(int profile,
                                              const BookmarkNode* parent) {
  ASSERT_EQ(GetBookmarkModel(profile)->GetNodeByID(parent->id()), parent)
      << "Node " << parent->GetTitle() << " does not belong to "
      << "Profile " << profile;
  verifier_helper_->ReverseChildOrder(GetBookmarkModel(profile), parent);
}

bool LiveBookmarksSyncTest::ModelMatchesVerifier(int profile) {
  if (verifier_helper_->use_verifier_model() == false) {
    LOG(ERROR) << "Illegal to call ModelMatchesVerifier() after "
               << "DisableVerifier(). Use ModelsMatch() instead.";
    return false;
  }
  return BookmarkModelVerifier::ModelsMatch(
      GetVerifierBookmarkModel(), GetBookmarkModel(profile));
}

bool LiveBookmarksSyncTest::AllModelsMatchVerifier() {
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

bool LiveBookmarksSyncTest::ModelsMatch(int profile_a, int profile_b) {
  return BookmarkModelVerifier::ModelsMatch(
      GetBookmarkModel(profile_a), GetBookmarkModel(profile_b));
}

bool LiveBookmarksSyncTest::AllModelsMatch() {
  for (int i = 1; i < num_clients(); ++i) {
    if (!ModelsMatch(0, i)) {
      LOG(ERROR) << "Model " << i << " does not match Model 0.";
      return false;
    }
  }
  return true;
}

bool LiveBookmarksSyncTest::ContainsDuplicateBookmarks(int profile) {
  return BookmarkModelVerifier::ContainsDuplicateBookmarks(
      GetBookmarkModel(profile));
}

const BookmarkNode* LiveBookmarksSyncTest::GetUniqueNodeByURL(int profile,
                                                              const GURL& url) {
  std::vector<const BookmarkNode*> nodes;
  GetBookmarkModel(profile)->GetNodesByURL(url, &nodes);
  EXPECT_EQ(1U, nodes.size());
  if (nodes.empty())
    return NULL;
  return nodes[0];
}

int LiveBookmarksSyncTest::CountBookmarksWithTitlesMatching(
    int profile,
    const std::wstring& title) {
  return verifier_helper_->CountNodesWithTitlesMatching(
      GetBookmarkModel(profile), BookmarkNode::URL, WideToUTF16(title));
}

int LiveBookmarksSyncTest::CountFoldersWithTitlesMatching(
    int profile,
    const std::wstring& title) {
  return verifier_helper_->CountNodesWithTitlesMatching(
      GetBookmarkModel(profile), BookmarkNode::FOLDER, WideToUTF16(title));
}

// static
std::vector<unsigned char> LiveBookmarksSyncTest::CreateFavicon(int seed) {
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
