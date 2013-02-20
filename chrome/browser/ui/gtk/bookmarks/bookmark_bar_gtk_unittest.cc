// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/bookmarks/bookmark_bar_gtk.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/gtk/tabstrip_origin_provider.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

// Dummy implementation that's good enough for the tests; we don't test
// rendering here so all we need is a non-NULL object.
class EmptyTabstripOriginProvider : public TabstripOriginProvider {
 public:
  virtual gfx::Point GetTabStripOriginForWidget(GtkWidget* widget) OVERRIDE {
    return gfx::Point(0, 0);
  }
};

class BookmarkBarGtkUnittest : public testing::Test {
 protected:
  BookmarkBarGtkUnittest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_) {
  }

  virtual void SetUp() OVERRIDE {
    profile_.reset(new TestingProfile());
    profile_->CreateBookmarkModel(true);
    profile_->BlockUntilBookmarkModelLoaded();
    model_ = BookmarkModelFactory::GetForProfile(profile_.get());

    Browser::CreateParams native_params(profile_.get(),
                                        chrome::HOST_DESKTOP_TYPE_NATIVE);
    browser_.reset(
        chrome::CreateBrowserWithTestWindowForParams(&native_params));
    origin_provider_.reset(new EmptyTabstripOriginProvider);
    bookmark_bar_.reset(new BookmarkBarGtk(NULL, browser_.get(),
                                           origin_provider_.get()));
  }

  virtual void TearDown() OVERRIDE {
    message_loop_.RunUntilIdle();

    bookmark_bar_.reset();
    origin_provider_.reset();
    browser_.reset();
    profile_.reset();
  }

  BookmarkModel* model_;

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<Browser> browser_;
  scoped_ptr<TabstripOriginProvider> origin_provider_;
  scoped_ptr<BookmarkBarGtk> bookmark_bar_;
};

TEST_F(BookmarkBarGtkUnittest, DisplaysHelpMessageOnEmpty) {
  bookmark_bar_->Loaded(model_, false);

  // There are no bookmarks in the model by default. Expect that the
  // |instructions_label| is shown.
  EXPECT_TRUE(bookmark_bar_->show_instructions_);
}

TEST_F(BookmarkBarGtkUnittest, HidesHelpMessageWithBookmark) {
  const BookmarkNode* parent = model_->bookmark_bar_node();
  model_->AddURL(parent, parent->child_count(),
                 ASCIIToUTF16("title"), GURL("http://one.com"));

  bookmark_bar_->Loaded(model_, false);
  EXPECT_FALSE(bookmark_bar_->show_instructions_);
}

TEST_F(BookmarkBarGtkUnittest, BuildsButtons) {
  const BookmarkNode* parent = model_->bookmark_bar_node();
  model_->AddURL(parent, parent->child_count(),
                 ASCIIToUTF16("title"), GURL("http://one.com"));
  model_->AddURL(parent, parent->child_count(),
                 ASCIIToUTF16("other"), GURL("http://two.com"));

  bookmark_bar_->Loaded(model_, false);

  // We should expect two children to the bookmark bar's toolbar.
  GList* children = gtk_container_get_children(
      GTK_CONTAINER(bookmark_bar_->bookmark_toolbar_.get()));
  EXPECT_EQ(2U, g_list_length(children));
  g_list_free(children);
}
