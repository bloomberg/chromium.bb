// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/android/partner_bookmarks_shim.h"

#include "base/message_loop.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

class PartnerBookmarksShimTest : public testing::Test {
 public:
  PartnerBookmarksShimTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        file_thread_(content::BrowserThread::FILE, &message_loop_),
        model_(NULL) {
  }

  TestingProfile* profile() const { return profile_.get(); }

  const BookmarkNode*  AddBookmark(const BookmarkNode* parent,
                                   const GURL& url,
                                   const string16& title) {
    if (!parent)
      parent = model_->bookmark_bar_node();
    return model_->AddURL(parent, parent->child_count(), title, url);
  }

  const BookmarkNode*  AddFolder(const BookmarkNode* parent,
                                 const string16& title) {
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
    ui_test_utils::WaitForBookmarkModelToLoad(model_);
  }

  virtual void TearDown() OVERRIDE {
    profile_.reset(NULL);
    PartnerBookmarksShim::GetInstance()->Reset();
  }

  scoped_ptr<TestingProfile> profile_;

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  BookmarkModel* model_;

  DISALLOW_COPY_AND_ASSIGN(PartnerBookmarksShimTest);
};

class TestObserver : public PartnerBookmarksShim::Observer {
 public:
  TestObserver()
      : notified_of_load_(false) {
  }

  // Called when everything is loaded and attached
  virtual void PartnerShimLoaded(PartnerBookmarksShim*) OVERRIDE {
    notified_of_load_ = true;
  }

  bool IsNotifiedOfLoad() { return notified_of_load_; }

 private:
  bool notified_of_load_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

TEST_F(PartnerBookmarksShimTest, GetNodeByID) {
  // Add a bookmark.
  GURL bookmark_url("http://www.google.com/");
  AddBookmark(
        NULL,
        bookmark_url,
        ASCIIToUTF16("bar"));
  const BookmarkNode* test_folder1 = AddFolder(NULL,
                                               ASCIIToUTF16("test_folder1"));
  const BookmarkNode* test_folder2 = AddFolder(test_folder1,
                                               ASCIIToUTF16("test_folder2"));
  const BookmarkNode* model_bookmark1 = AddBookmark(test_folder2,
                                                    GURL("http://www.test.com"),
                                                    ASCIIToUTF16("bar"));
  const BookmarkNode* model_bookmark2 = AddBookmark(test_folder2,
                                                    GURL("http://www.foo.com"),
                                                    ASCIIToUTF16("baz"));

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

  PartnerBookmarksShim* shim = PartnerBookmarksShim::GetInstance();
  ASSERT_FALSE(shim->IsLoaded());
  shim->AttachTo(model_, model_->mobile_node());
  shim->SetPartnerBookmarksRoot(root_partner_node);
  ASSERT_TRUE(shim->IsLoaded());

  ASSERT_EQ(shim->GetNodeByID(model_bookmark1->id(), false), model_bookmark1);
  ASSERT_EQ(shim->GetNodeByID(model_bookmark2->id(), false), model_bookmark2);
  ASSERT_EQ(shim->GetNodeByID(test_folder2->id(), false), test_folder2);
  ASSERT_EQ(shim->GetNodeByID(0, true), root_partner_node);
  ASSERT_EQ(shim->GetNodeByID(1, true), partner_folder1);
  ASSERT_EQ(shim->GetNodeByID(4, true), partner_bookmark2);
}

TEST_F(PartnerBookmarksShimTest, ObserverNotifiedOfLoadNoPartnerBookmarks) {
  TestObserver* observer = new TestObserver();
  PartnerBookmarksShim* shim = PartnerBookmarksShim::GetInstance();
  shim->AddObserver(observer);
  ASSERT_FALSE(observer->IsNotifiedOfLoad());
  shim->SetPartnerBookmarksRoot(NULL);
  ASSERT_TRUE(observer->IsNotifiedOfLoad());
}

TEST_F(PartnerBookmarksShimTest, ObserverNotifiedOfLoadWithPartnerBookmarks) {
  int64 id = 5;
  BookmarkNode* root_partner_node = new BookmarkPermanentNode(id++);
  BookmarkNode* partner_bookmark1 = new BookmarkNode(id++,
                                                     GURL("http://www.a.com"));
  partner_bookmark1->set_type(BookmarkNode::URL);
  root_partner_node->Add(partner_bookmark1, root_partner_node->child_count());

  TestObserver* observer = new TestObserver();
  PartnerBookmarksShim* shim = PartnerBookmarksShim::GetInstance();
  shim->AddObserver(observer);
  shim->AttachTo(model_, model_->bookmark_bar_node());
  ASSERT_FALSE(observer->IsNotifiedOfLoad());
  shim->SetPartnerBookmarksRoot(root_partner_node);
  ASSERT_TRUE(observer->IsNotifiedOfLoad());
}
