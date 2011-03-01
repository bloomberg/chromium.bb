// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/bookmarks/bookmark_bar_gtk.h"

#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/ui/gtk/tabstrip_origin_provider.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

// Dummy implementation that's good enough for the tests; we don't test
// rendering here so all we need is a non-NULL object.
class EmptyTabstripOriginProvider : public TabstripOriginProvider {
 public:
  virtual gfx::Point GetTabStripOriginForWidget(GtkWidget* widget) {
    return gfx::Point(0, 0);
  }
};

class BookmarkBarGtkUnittest : public ::testing::Test {
 protected:
  BookmarkBarGtkUnittest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_) {
  }

  virtual void SetUp() {
    profile_.reset(new TestingProfile());
    profile_->CreateBookmarkModel(true);
    profile_->BlockUntilBookmarkModelLoaded();
    browser_.reset(new Browser(Browser::TYPE_NORMAL, profile_.get()));

    origin_provider_.reset(new EmptyTabstripOriginProvider);
    bookmark_bar_.reset(new BookmarkBarGtk(NULL, profile_.get(), browser_.get(),
                                           origin_provider_.get()));
  }

  virtual void TearDown() {
    message_loop_.RunAllPending();

    bookmark_bar_.reset();
    origin_provider_.reset();
    browser_.reset();
    profile_.reset();
  }

  MessageLoopForUI message_loop_;
  BrowserThread ui_thread_;
  BrowserThread file_thread_;

  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<Browser> browser_;
  scoped_ptr<TabstripOriginProvider> origin_provider_;
  scoped_ptr<BookmarkBarGtk> bookmark_bar_;
};

TEST_F(BookmarkBarGtkUnittest, DisplaysHelpMessageOnEmpty) {
  BookmarkModel* model = profile_->GetBookmarkModel();
  bookmark_bar_->Loaded(model);

  // There are no bookmarks in the model by default. Expect that the
  // |instructions_label| is shown.
  EXPECT_TRUE(bookmark_bar_->show_instructions_);
}

TEST_F(BookmarkBarGtkUnittest, HidesHelpMessageWithBookmark) {
  BookmarkModel* model = profile_->GetBookmarkModel();

  const BookmarkNode* parent = model->GetBookmarkBarNode();
  model->AddURL(parent, parent->GetChildCount(),
                ASCIIToUTF16("title"), GURL("http://one.com"));

  bookmark_bar_->Loaded(model);
  EXPECT_FALSE(bookmark_bar_->show_instructions_);
}

TEST_F(BookmarkBarGtkUnittest, BuildsButtons) {
  BookmarkModel* model = profile_->GetBookmarkModel();

  const BookmarkNode* parent = model->GetBookmarkBarNode();
  model->AddURL(parent, parent->GetChildCount(),
                ASCIIToUTF16("title"), GURL("http://one.com"));
  model->AddURL(parent, parent->GetChildCount(),
                ASCIIToUTF16("other"), GURL("http://two.com"));

  bookmark_bar_->Loaded(model);

  // We should expect two children to the bookmark bar's toolbar.
  GList* children = gtk_container_get_children(
      GTK_CONTAINER(bookmark_bar_->bookmark_toolbar_.get()));
  EXPECT_EQ(2U, g_list_length(children));
  g_list_free(children);
}
