// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_node_data.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_win.h"

using content::BrowserThread;

class BookmarkNodeDataTest : public testing::Test {
 public:
  BookmarkNodeDataTest()
      : ui_thread_(BrowserThread::UI, &loop_),
        file_thread_(BrowserThread::FILE, &loop_) {
  }

 private:
  MessageLoop loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
};

namespace {

ui::OSExchangeData::Provider* CloneProvider(const ui::OSExchangeData& data) {
  return new ui::OSExchangeDataProviderWin(
      ui::OSExchangeDataProviderWin::GetIDataObject(data));
}

}  // namespace

// Makes sure BookmarkNodeData is initially invalid.
TEST_F(BookmarkNodeDataTest, InitialState) {
  BookmarkNodeData data;
  EXPECT_FALSE(data.is_valid());
}

// Makes sure reading bogus data leaves the BookmarkNodeData invalid.
TEST_F(BookmarkNodeDataTest, BogusRead) {
  ui::OSExchangeData data;
  BookmarkNodeData drag_data;
  EXPECT_FALSE(drag_data.Read(ui::OSExchangeData(CloneProvider(data))));
  EXPECT_FALSE(drag_data.is_valid());
}

// Writes a URL to the clipboard and make sure BookmarkNodeData can correctly
// read it.
TEST_F(BookmarkNodeDataTest, JustURL) {
  const GURL url("http://google.com");
  const string16 title(ASCIIToUTF16("google.com"));

  ui::OSExchangeData data;
  data.SetURL(url, title);

  BookmarkNodeData drag_data;
  EXPECT_TRUE(drag_data.Read(ui::OSExchangeData(CloneProvider(data))));
  EXPECT_TRUE(drag_data.is_valid());
  ASSERT_EQ(1, drag_data.elements.size());
  EXPECT_TRUE(drag_data.elements[0].is_url);
  EXPECT_EQ(url, drag_data.elements[0].url);
  EXPECT_EQ(title, drag_data.elements[0].title);
  EXPECT_EQ(0, drag_data.elements[0].children.size());
}

TEST_F(BookmarkNodeDataTest, URL) {
  // Write a single node representing a URL to the clipboard.
  TestingProfile profile;
  profile.CreateBookmarkModel(false);
  profile.BlockUntilBookmarkModelLoaded();
  profile.SetID(L"id");
  BookmarkModel* model = BookmarkModelFactory::GetForProfile(&profile);
  const BookmarkNode* root = model->bookmark_bar_node();
  GURL url(GURL("http://foo.com"));
  const string16 title(ASCIIToUTF16("foo.com"));
  const BookmarkNode* node = model->AddURL(root, 0, title, url);
  BookmarkNodeData drag_data(node);
  EXPECT_TRUE(drag_data.is_valid());
  ASSERT_EQ(1, drag_data.elements.size());
  EXPECT_TRUE(drag_data.elements[0].is_url);
  EXPECT_EQ(url, drag_data.elements[0].url);
  EXPECT_EQ(title, drag_data.elements[0].title);
  ui::OSExchangeData data;
  drag_data.Write(&profile, &data);

  // Now read the data back in.
  ui::OSExchangeData data2(CloneProvider(data));
  BookmarkNodeData read_data;
  EXPECT_TRUE(read_data.Read(data2));
  EXPECT_TRUE(read_data.is_valid());
  ASSERT_EQ(1, read_data.elements.size());
  EXPECT_TRUE(read_data.elements[0].is_url);
  EXPECT_EQ(url, read_data.elements[0].url);
  EXPECT_EQ(title, read_data.elements[0].title);
  EXPECT_TRUE(read_data.GetFirstNode(&profile) == node);

  // Make sure asking for the node with a different profile returns NULL.
  TestingProfile profile2;
  EXPECT_TRUE(read_data.GetFirstNode(&profile2) == NULL);

  // Writing should also put the URL and title on the clipboard.
  GURL read_url;
  string16 read_title;
  EXPECT_TRUE(data2.GetURLAndTitle(&read_url, &read_title));
  EXPECT_EQ(url, read_url);
  EXPECT_EQ(title, read_title);
}

// Tests writing a folder to the clipboard.
TEST_F(BookmarkNodeDataTest, Folder) {
  TestingProfile profile;
  profile.CreateBookmarkModel(false);
  profile.BlockUntilBookmarkModelLoaded();
  profile.SetID(L"id");
  BookmarkModel* model = BookmarkModelFactory::GetForProfile(&profile);
  const BookmarkNode* root = model->bookmark_bar_node();
  const BookmarkNode* g1 = model->AddFolder(root, 0, ASCIIToUTF16("g1"));
  const BookmarkNode* g11 = model->AddFolder(g1, 0, ASCIIToUTF16("g11"));
  const BookmarkNode* g12 = model->AddFolder(g1, 0, ASCIIToUTF16("g12"));

  BookmarkNodeData drag_data(g12);
  EXPECT_TRUE(drag_data.is_valid());
  ASSERT_EQ(1, drag_data.elements.size());
  EXPECT_EQ(g12->GetTitle(), drag_data.elements[0].title);
  EXPECT_FALSE(drag_data.elements[0].is_url);

  ui::OSExchangeData data;
  drag_data.Write(&profile, &data);

  // Now read the data back in.
  ui::OSExchangeData data2(CloneProvider(data));
  BookmarkNodeData read_data;
  EXPECT_TRUE(read_data.Read(data2));
  EXPECT_TRUE(read_data.is_valid());
  ASSERT_EQ(1, read_data.elements.size());
  EXPECT_EQ(g12->GetTitle(), read_data.elements[0].title);
  EXPECT_FALSE(read_data.elements[0].is_url);

  // We should get back the same node when asking for the same profile.
  const BookmarkNode* r_g12 = read_data.GetFirstNode(&profile);
  EXPECT_TRUE(g12 == r_g12);

  // A different profile should return NULL for the node.
  TestingProfile profile2;
  EXPECT_TRUE(read_data.GetFirstNode(&profile2) == NULL);
}

// Tests reading/writing a folder with children.
TEST_F(BookmarkNodeDataTest, FolderWithChild) {
  TestingProfile profile;
  profile.SetID(L"id");
  profile.CreateBookmarkModel(false);
  profile.BlockUntilBookmarkModelLoaded();
  BookmarkModel* model = BookmarkModelFactory::GetForProfile(&profile);
  const BookmarkNode* root = model->bookmark_bar_node();
  const BookmarkNode* folder = model->AddFolder(root, 0, ASCIIToUTF16("g1"));

  GURL url(GURL("http://foo.com"));
  const string16 title(ASCIIToUTF16("blah2"));

  model->AddURL(folder, 0, title, url);

  BookmarkNodeData drag_data(folder);

  ui::OSExchangeData data;
  drag_data.Write(&profile, &data);

  // Now read the data back in.
  ui::OSExchangeData data2(CloneProvider(data));
  BookmarkNodeData read_data;
  EXPECT_TRUE(read_data.Read(data2));
  ASSERT_EQ(1, read_data.elements.size());
  ASSERT_EQ(1, read_data.elements[0].children.size());
  const BookmarkNodeData::Element& read_child =
      read_data.elements[0].children[0];

  EXPECT_TRUE(read_child.is_url);
  EXPECT_EQ(title, read_child.title);
  EXPECT_EQ(url, read_child.url);
  EXPECT_TRUE(read_child.is_url);

  // And make sure we get the node back.
  const BookmarkNode* r_folder = read_data.GetFirstNode(&profile);
  EXPECT_TRUE(folder == r_folder);
}

// Tests reading/writing of multiple nodes.
TEST_F(BookmarkNodeDataTest, MultipleNodes) {
  TestingProfile profile;
  profile.SetID(L"id");
  profile.CreateBookmarkModel(false);
  profile.BlockUntilBookmarkModelLoaded();
  BookmarkModel* model = BookmarkModelFactory::GetForProfile(&profile);
  const BookmarkNode* root = model->bookmark_bar_node();
  const BookmarkNode* folder = model->AddFolder(root, 0, ASCIIToUTF16("g1"));

  GURL url(GURL("http://foo.com"));
  const string16 title(ASCIIToUTF16("blah2"));

  const BookmarkNode* url_node = model->AddURL(folder, 0, title, url);

  // Write the nodes to the clipboard.
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(folder);
  nodes.push_back(url_node);
  BookmarkNodeData drag_data(nodes);
  ui::OSExchangeData data;
  drag_data.Write(&profile, &data);

  // Read the data back in.
  ui::OSExchangeData data2(CloneProvider(data));
  BookmarkNodeData read_data;
  EXPECT_TRUE(read_data.Read(data2));
  EXPECT_TRUE(read_data.is_valid());
  ASSERT_EQ(2, read_data.elements.size());
  ASSERT_EQ(1, read_data.elements[0].children.size());

  const BookmarkNodeData::Element& read_folder = read_data.elements[0];
  EXPECT_FALSE(read_folder.is_url);
  EXPECT_EQ(L"g1", read_folder.title);
  EXPECT_EQ(1, read_folder.children.size());

  const BookmarkNodeData::Element& read_url = read_data.elements[1];
  EXPECT_TRUE(read_url.is_url);
  EXPECT_EQ(title, read_url.title);
  EXPECT_EQ(0, read_url.children.size());

  // And make sure we get the node back.
  std::vector<const BookmarkNode*> read_nodes = read_data.GetNodes(&profile);
  ASSERT_EQ(2, read_nodes.size());
  EXPECT_TRUE(read_nodes[0] == folder);
  EXPECT_TRUE(read_nodes[1] == url_node);

  // Asking for the first node should return NULL with more than one element
  // present.
  EXPECT_TRUE(read_data.GetFirstNode(&profile) == NULL);
}
