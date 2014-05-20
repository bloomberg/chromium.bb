// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/bookmarks/partner_bookmarks_shim.h"

#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;

class MockObserver : public PartnerBookmarksShim::Observer {
 public:
  MockObserver() {}
  MOCK_METHOD1(PartnerShimChanged, void(PartnerBookmarksShim*));
  MOCK_METHOD1(PartnerShimLoaded, void(PartnerBookmarksShim*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockObserver);
};

class PartnerBookmarksShimTest : public testing::Test {
 public:
  PartnerBookmarksShimTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        file_thread_(content::BrowserThread::FILE, &message_loop_),
        model_(NULL) {
  }

  TestingProfile* profile() const { return profile_.get(); }
  PartnerBookmarksShim* partner_bookmarks_shim() const {
    return PartnerBookmarksShim::BuildForBrowserContext(profile_.get());
  }

  const BookmarkNode* AddBookmark(const BookmarkNode* parent,
                                  const GURL& url,
                                  const base::string16& title) {
    if (!parent)
      parent = model_->bookmark_bar_node();
    return model_->AddURL(parent, parent->child_count(), title, url);
  }

  const BookmarkNode* AddFolder(const BookmarkNode* parent,
                                const base::string16& title) {
    if (!parent)
      parent = model_->bookmark_bar_node();
    return model_->AddFolder(parent, parent->child_count(), title);
  }

 protected:
  // testing::Test
  virtual void SetUp() OVERRIDE {
    profile_.reset(new TestingProfile());
    profile_->CreateBookmarkModel(true);

    model_ = BookmarkModelFactory::GetForProfile(profile_.get());
    test::WaitForBookmarkModelToLoad(model_);
  }

  virtual void TearDown() OVERRIDE {
    PartnerBookmarksShim::ClearInBrowserContextForTesting(profile_.get());
    PartnerBookmarksShim::ClearPartnerModelForTesting();
    PartnerBookmarksShim::EnablePartnerBookmarksEditing();
    profile_.reset(NULL);
  }

  scoped_ptr<TestingProfile> profile_;

  base::MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  BookmarkModel* model_;
  MockObserver observer_;

  DISALLOW_COPY_AND_ASSIGN(PartnerBookmarksShimTest);
};

TEST_F(PartnerBookmarksShimTest, GetNodeByID) {
  BookmarkNode* root_partner_node = new BookmarkPermanentNode(0);
  BookmarkNode* partner_folder1 = new BookmarkNode(1, GURL());
  partner_folder1->set_type(BookmarkNode::FOLDER);
  root_partner_node->Add(partner_folder1, root_partner_node->child_count());

  BookmarkNode* partner_folder2 = new BookmarkNode(2, GURL());
  partner_folder2->set_type(BookmarkNode::FOLDER);
  partner_folder1->Add(partner_folder2, partner_folder1->child_count());

  BookmarkNode* partner_bookmark1 = new BookmarkNode(3,
                                                     GURL("http://www.a.com"));
  partner_bookmark1->set_type(BookmarkNode::URL);
  partner_folder1->Add(partner_bookmark1, partner_folder1->child_count());

  BookmarkNode* partner_bookmark2 = new BookmarkNode(4,
                                                     GURL("http://www.b.com"));
  partner_bookmark2->set_type(BookmarkNode::URL);
  partner_folder2->Add(partner_bookmark2, partner_folder2->child_count());

  PartnerBookmarksShim* shim = partner_bookmarks_shim();
  ASSERT_FALSE(shim->IsLoaded());
  shim->SetPartnerBookmarksRoot(root_partner_node);
  ASSERT_TRUE(shim->IsLoaded());

  ASSERT_TRUE(shim->IsPartnerBookmark(root_partner_node));
  ASSERT_EQ(shim->GetNodeByID(0), root_partner_node);
  ASSERT_EQ(shim->GetNodeByID(1), partner_folder1);
  ASSERT_EQ(shim->GetNodeByID(4), partner_bookmark2);
}

TEST_F(PartnerBookmarksShimTest, ObserverNotifiedOfLoadNoPartnerBookmarks) {
  EXPECT_CALL(observer_, PartnerShimLoaded(_)).Times(0);
  PartnerBookmarksShim* shim = partner_bookmarks_shim();
  shim->AddObserver(&observer_);

  EXPECT_CALL(observer_, PartnerShimLoaded(shim)).Times(1);
  shim->SetPartnerBookmarksRoot(NULL);
}

TEST_F(PartnerBookmarksShimTest, ObserverNotifiedOfLoadWithPartnerBookmarks) {
  EXPECT_CALL(observer_, PartnerShimLoaded(_)).Times(0);
  int64 id = 5;
  BookmarkNode* root_partner_node = new BookmarkPermanentNode(id++);
  BookmarkNode* partner_bookmark1 = new BookmarkNode(id++,
                                                     GURL("http://www.a.com"));
  partner_bookmark1->set_type(BookmarkNode::URL);
  root_partner_node->Add(partner_bookmark1, root_partner_node->child_count());

  PartnerBookmarksShim* shim = partner_bookmarks_shim();
  shim->AddObserver(&observer_);

  EXPECT_CALL(observer_, PartnerShimLoaded(shim)).Times(1);
  shim->SetPartnerBookmarksRoot(root_partner_node);
}

TEST_F(PartnerBookmarksShimTest, RemoveBookmarks) {
  PartnerBookmarksShim* shim = partner_bookmarks_shim();
  shim->AddObserver(&observer_);

  EXPECT_CALL(observer_, PartnerShimLoaded(shim)).Times(0);
  EXPECT_CALL(observer_, PartnerShimChanged(shim)).Times(0);

  BookmarkNode* root_partner_node = new BookmarkPermanentNode(0);
  root_partner_node->SetTitle(base::ASCIIToUTF16("Partner bookmarks"));

  BookmarkNode* partner_folder1 = new BookmarkNode(1, GURL("http://www.a.net"));
  partner_folder1->set_type(BookmarkNode::FOLDER);
  root_partner_node->Add(partner_folder1, root_partner_node->child_count());

  BookmarkNode* partner_folder2 = new BookmarkNode(2, GURL("http://www.b.net"));
  partner_folder2->set_type(BookmarkNode::FOLDER);
  root_partner_node->Add(partner_folder2, root_partner_node->child_count());

  BookmarkNode* partner_bookmark1 = new BookmarkNode(3,
                                                     GURL("http://www.a.com"));
  partner_bookmark1->set_type(BookmarkNode::URL);
  partner_folder1->Add(partner_bookmark1, partner_folder1->child_count());

  BookmarkNode* partner_bookmark2 = new BookmarkNode(4,
                                                     GURL("http://www.b.com"));
  partner_bookmark2->set_type(BookmarkNode::URL);
  partner_folder2->Add(partner_bookmark2, partner_folder2->child_count());

  BookmarkNode* partner_folder3 = new BookmarkNode(5, GURL("http://www.c.net"));
  partner_folder3->set_type(BookmarkNode::FOLDER);
  partner_folder2->Add(partner_folder3, partner_folder2->child_count());

  BookmarkNode* partner_bookmark3 = new BookmarkNode(6,
                                                     GURL("http://www.c.com"));
  partner_bookmark3->set_type(BookmarkNode::URL);
  partner_folder3->Add(partner_bookmark3, partner_folder3->child_count());

  ASSERT_FALSE(shim->IsLoaded());
  EXPECT_CALL(observer_, PartnerShimLoaded(shim)).Times(1);
  shim->SetPartnerBookmarksRoot(root_partner_node);
  ASSERT_TRUE(shim->IsLoaded());

  EXPECT_EQ(root_partner_node, shim->GetNodeByID(0));
  EXPECT_EQ(partner_folder1, shim->GetNodeByID(1));
  EXPECT_EQ(partner_folder2, shim->GetNodeByID(2));
  EXPECT_EQ(partner_bookmark1, shim->GetNodeByID(3));
  EXPECT_EQ(partner_bookmark2, shim->GetNodeByID(4));
  EXPECT_EQ(partner_folder3, shim->GetNodeByID(5));
  EXPECT_EQ(partner_bookmark3, shim->GetNodeByID(6));

  EXPECT_TRUE(shim->IsReachable(root_partner_node));
  EXPECT_TRUE(shim->IsReachable(partner_folder1));
  EXPECT_TRUE(shim->IsReachable(partner_folder2));
  EXPECT_TRUE(shim->IsReachable(partner_bookmark1));
  EXPECT_TRUE(shim->IsReachable(partner_bookmark2));
  EXPECT_TRUE(shim->IsReachable(partner_folder3));
  EXPECT_TRUE(shim->IsReachable(partner_bookmark3));

  EXPECT_CALL(observer_, PartnerShimChanged(shim)).Times(1);
  shim->RemoveBookmark(partner_bookmark2);
  EXPECT_TRUE(shim->IsReachable(root_partner_node));
  EXPECT_TRUE(shim->IsReachable(partner_folder1));
  EXPECT_TRUE(shim->IsReachable(partner_folder2));
  EXPECT_TRUE(shim->IsReachable(partner_bookmark1));
  EXPECT_FALSE(shim->IsReachable(partner_bookmark2));
  EXPECT_TRUE(shim->IsReachable(partner_folder3));
  EXPECT_TRUE(shim->IsReachable(partner_bookmark3));

  EXPECT_CALL(observer_, PartnerShimChanged(shim)).Times(1);
  shim->RemoveBookmark(partner_folder1);
  EXPECT_TRUE(shim->IsReachable(root_partner_node));
  EXPECT_FALSE(shim->IsReachable(partner_folder1));
  EXPECT_TRUE(shim->IsReachable(partner_folder2));
  EXPECT_FALSE(shim->IsReachable(partner_bookmark1));
  EXPECT_FALSE(shim->IsReachable(partner_bookmark2));
  EXPECT_TRUE(shim->IsReachable(partner_folder3));
  EXPECT_TRUE(shim->IsReachable(partner_bookmark3));

  EXPECT_CALL(observer_, PartnerShimChanged(shim)).Times(1);
  shim->RemoveBookmark(root_partner_node);
  EXPECT_FALSE(shim->IsReachable(root_partner_node));
  EXPECT_FALSE(shim->IsReachable(partner_folder1));
  EXPECT_FALSE(shim->IsReachable(partner_folder2));
  EXPECT_FALSE(shim->IsReachable(partner_bookmark1));
  EXPECT_FALSE(shim->IsReachable(partner_bookmark2));
  EXPECT_FALSE(shim->IsReachable(partner_folder3));
  EXPECT_FALSE(shim->IsReachable(partner_bookmark3));
}

TEST_F(PartnerBookmarksShimTest, RenameBookmarks) {
  PartnerBookmarksShim* shim = partner_bookmarks_shim();
  shim->AddObserver(&observer_);

  EXPECT_CALL(observer_, PartnerShimLoaded(shim)).Times(0);
  EXPECT_CALL(observer_, PartnerShimChanged(shim)).Times(0);

  BookmarkNode* root_partner_node = new BookmarkPermanentNode(0);
  root_partner_node->SetTitle(base::ASCIIToUTF16("Partner bookmarks"));

  BookmarkNode* partner_folder1 = new BookmarkNode(1, GURL("http://www.a.net"));
  partner_folder1->set_type(BookmarkNode::FOLDER);
  partner_folder1->SetTitle(base::ASCIIToUTF16("a.net"));
  root_partner_node->Add(partner_folder1, root_partner_node->child_count());

  BookmarkNode* partner_folder2 = new BookmarkNode(2, GURL("http://www.b.net"));
  partner_folder2->set_type(BookmarkNode::FOLDER);
  partner_folder2->SetTitle(base::ASCIIToUTF16("b.net"));
  root_partner_node->Add(partner_folder2, root_partner_node->child_count());

  BookmarkNode* partner_bookmark1 = new BookmarkNode(3,
                                                     GURL("http://www.a.com"));
  partner_bookmark1->set_type(BookmarkNode::URL);
  partner_bookmark1->SetTitle(base::ASCIIToUTF16("a.com"));
  partner_folder1->Add(partner_bookmark1, partner_folder1->child_count());

  BookmarkNode* partner_bookmark2 = new BookmarkNode(4,
                                                     GURL("http://www.b.com"));
  partner_bookmark2->set_type(BookmarkNode::URL);
  partner_bookmark2->SetTitle(base::ASCIIToUTF16("b.com"));
  partner_folder2->Add(partner_bookmark2, partner_folder2->child_count());

  ASSERT_FALSE(shim->IsLoaded());
  EXPECT_CALL(observer_, PartnerShimLoaded(shim)).Times(1);
  shim->SetPartnerBookmarksRoot(root_partner_node);
  ASSERT_TRUE(shim->IsLoaded());

  EXPECT_EQ(root_partner_node, shim->GetNodeByID(0));
  EXPECT_EQ(partner_folder1, shim->GetNodeByID(1));
  EXPECT_EQ(partner_folder2, shim->GetNodeByID(2));
  EXPECT_EQ(partner_bookmark1, shim->GetNodeByID(3));
  EXPECT_EQ(partner_bookmark2, shim->GetNodeByID(4));

  EXPECT_TRUE(shim->IsReachable(root_partner_node));
  EXPECT_TRUE(shim->IsReachable(partner_folder1));
  EXPECT_TRUE(shim->IsReachable(partner_folder2));
  EXPECT_TRUE(shim->IsReachable(partner_bookmark1));
  EXPECT_TRUE(shim->IsReachable(partner_bookmark2));

  EXPECT_CALL(observer_, PartnerShimChanged(shim)).Times(1);
  EXPECT_EQ(base::ASCIIToUTF16("b.com"), shim->GetTitle(partner_bookmark2));
  shim->RenameBookmark(partner_bookmark2, base::ASCIIToUTF16("b2.com"));
  EXPECT_EQ(base::ASCIIToUTF16("b2.com"), shim->GetTitle(partner_bookmark2));

  EXPECT_TRUE(shim->IsReachable(root_partner_node));
  EXPECT_TRUE(shim->IsReachable(partner_folder1));
  EXPECT_TRUE(shim->IsReachable(partner_folder2));
  EXPECT_TRUE(shim->IsReachable(partner_bookmark1));
  EXPECT_TRUE(shim->IsReachable(partner_bookmark2));

  EXPECT_CALL(observer_, PartnerShimChanged(shim)).Times(1);
  EXPECT_EQ(base::ASCIIToUTF16("a.net"), shim->GetTitle(partner_folder1));
  shim->RenameBookmark(partner_folder1, base::ASCIIToUTF16("a2.net"));
  EXPECT_EQ(base::ASCIIToUTF16("a2.net"), shim->GetTitle(partner_folder1));

  EXPECT_TRUE(shim->IsReachable(root_partner_node));
  EXPECT_TRUE(shim->IsReachable(partner_folder1));
  EXPECT_TRUE(shim->IsReachable(partner_folder2));
  EXPECT_TRUE(shim->IsReachable(partner_bookmark1));
  EXPECT_TRUE(shim->IsReachable(partner_bookmark2));

  EXPECT_CALL(observer_, PartnerShimChanged(shim)).Times(1);
  EXPECT_EQ(base::ASCIIToUTF16("Partner bookmarks"),
            shim->GetTitle(root_partner_node));
  shim->RenameBookmark(root_partner_node, base::ASCIIToUTF16("Partner"));
  EXPECT_EQ(base::ASCIIToUTF16("Partner"), shim->GetTitle(root_partner_node));

  EXPECT_TRUE(shim->IsReachable(root_partner_node));
  EXPECT_TRUE(shim->IsReachable(partner_folder1));
  EXPECT_TRUE(shim->IsReachable(partner_folder2));
  EXPECT_TRUE(shim->IsReachable(partner_bookmark1));
  EXPECT_TRUE(shim->IsReachable(partner_bookmark2));
}

TEST_F(PartnerBookmarksShimTest, SaveLoadProfile) {
  {
    PartnerBookmarksShim* shim = partner_bookmarks_shim();
    shim->AddObserver(&observer_);

    EXPECT_CALL(observer_, PartnerShimLoaded(shim)).Times(0);
    EXPECT_CALL(observer_, PartnerShimChanged(shim)).Times(0);

    BookmarkNode* root_partner_node = new BookmarkPermanentNode(0);
    root_partner_node->SetTitle(base::ASCIIToUTF16("Partner bookmarks"));

    BookmarkNode* partner_folder1 = new BookmarkNode(1, GURL("http://a.net"));
    partner_folder1->set_type(BookmarkNode::FOLDER);
    partner_folder1->SetTitle(base::ASCIIToUTF16("a.net"));
    root_partner_node->Add(partner_folder1, root_partner_node->child_count());

    BookmarkNode* partner_bookmark1 = new BookmarkNode(3,
                                                       GURL("http://a.com"));
    partner_bookmark1->set_type(BookmarkNode::URL);
    partner_bookmark1->SetTitle(base::ASCIIToUTF16("a.com"));
    partner_folder1->Add(partner_bookmark1, partner_folder1->child_count());

    BookmarkNode* partner_bookmark2 = new BookmarkNode(5,
                                                       GURL("http://b.com"));
    partner_bookmark2->set_type(BookmarkNode::URL);
    partner_bookmark2->SetTitle(base::ASCIIToUTF16("b.com"));
    partner_folder1->Add(partner_bookmark2, partner_folder1->child_count());

    ASSERT_FALSE(shim->IsLoaded());
    EXPECT_CALL(observer_, PartnerShimLoaded(shim)).Times(1);
    shim->SetPartnerBookmarksRoot(root_partner_node);
    ASSERT_TRUE(shim->IsLoaded());

    EXPECT_CALL(observer_, PartnerShimChanged(shim)).Times(2);
    shim->RenameBookmark(partner_bookmark1, base::ASCIIToUTF16("a2.com"));
    shim->RemoveBookmark(partner_bookmark2);
    EXPECT_EQ(base::ASCIIToUTF16("a2.com"), shim->GetTitle(partner_bookmark1));
    EXPECT_FALSE(shim->IsReachable(partner_bookmark2));
  }

  PartnerBookmarksShim::ClearInBrowserContextForTesting(profile_.get());

  {
    PartnerBookmarksShim* shim = partner_bookmarks_shim();
    shim->AddObserver(&observer_);

    EXPECT_CALL(observer_, PartnerShimLoaded(shim)).Times(0);
    EXPECT_CALL(observer_, PartnerShimChanged(shim)).Times(0);
    ASSERT_TRUE(shim->IsLoaded());

    const BookmarkNode* partner_bookmark1 = shim->GetNodeByID(3);
    const BookmarkNode* partner_bookmark2 = shim->GetNodeByID(5);

    EXPECT_EQ(base::ASCIIToUTF16("a2.com"), shim->GetTitle(partner_bookmark1));
    EXPECT_FALSE(shim->IsReachable(partner_bookmark2));
  }
}

TEST_F(PartnerBookmarksShimTest, DisableEditing) {
  PartnerBookmarksShim* shim = partner_bookmarks_shim();
  shim->AddObserver(&observer_);

  EXPECT_CALL(observer_, PartnerShimLoaded(shim)).Times(0);
  EXPECT_CALL(observer_, PartnerShimChanged(shim)).Times(0);

  BookmarkNode* root_partner_node = new BookmarkPermanentNode(0);
  root_partner_node->SetTitle(base::ASCIIToUTF16("Partner bookmarks"));

  BookmarkNode* partner_bookmark1 = new BookmarkNode(3, GURL("http://a"));
  partner_bookmark1->set_type(BookmarkNode::URL);
  partner_bookmark1->SetTitle(base::ASCIIToUTF16("a"));
  root_partner_node->Add(partner_bookmark1, root_partner_node->child_count());

  BookmarkNode* partner_bookmark2 = new BookmarkNode(3, GURL("http://b"));
  partner_bookmark2->set_type(BookmarkNode::URL);
  partner_bookmark2->SetTitle(base::ASCIIToUTF16("b"));
  root_partner_node->Add(partner_bookmark2, root_partner_node->child_count());

  ASSERT_FALSE(shim->IsLoaded());
  EXPECT_CALL(observer_, PartnerShimLoaded(shim)).Times(1);
  shim->SetPartnerBookmarksRoot(root_partner_node);
  ASSERT_TRUE(shim->IsLoaded());

  // Check that edits work by default.
  EXPECT_CALL(observer_, PartnerShimChanged(shim)).Times(2);
  shim->RenameBookmark(partner_bookmark1, base::ASCIIToUTF16("a2.com"));
  shim->RemoveBookmark(partner_bookmark2);
  EXPECT_EQ(base::ASCIIToUTF16("a2.com"), shim->GetTitle(partner_bookmark1));
  EXPECT_FALSE(shim->IsReachable(partner_bookmark2));

  // Disable edits and check that edits are not applied anymore.
  PartnerBookmarksShim::DisablePartnerBookmarksEditing();
  EXPECT_EQ(base::ASCIIToUTF16("a"), shim->GetTitle(partner_bookmark1));
  EXPECT_TRUE(shim->IsReachable(partner_bookmark2));
}
