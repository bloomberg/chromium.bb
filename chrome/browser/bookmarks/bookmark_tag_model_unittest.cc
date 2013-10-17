// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_tag_model.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_tag_model_observer.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace {

static struct {
  const std::string input_tag;
  const std::string expected_tag;
} whitespace_test_cases[] = {
  // Newlines.
  {"foo\nbar", "foo bar"},
  {"foo\n\nbar", "foo bar"},
  {"foo\n\n\nbar", "foo bar"},
  {"foo\r\nbar", "foo bar"},
  {"foo\r\n\r\nbar", "foo bar"},
  {"\nfoo\nbar\n", "foo bar"},
  // Spaces.
  {"foo  bar", "foo bar"},
  {" foo bar ", "foo bar"},
  {"  foo  bar  ", "foo bar"},
  // Tabs.
  {"\tfoo\tbar\t", "foo bar"},
  {"\tfoo bar\t", "foo bar"},
  // Mixed cases.
  {"\tfoo\nbar\t", "foo bar"},
  {"\tfoo\r\nbar\t", "foo bar"},
  {"  foo\tbar\n", "foo bar"},
  {"\t foo \t  bar  \t", "foo bar"},
  {"\n foo\r\n\tbar\n \t", "foo bar"},
};

enum ObserverCounts {
  OBSERVER_COUNTS_ADD = 0,
  OBSERVER_COUNTS_BEFORE_REMOVE,
  OBSERVER_COUNTS_REMOVE,
  OBSERVER_COUNTS_BEFORE_CHANGE,
  OBSERVER_COUNTS_CHANGE,
  OBSERVER_COUNTS_BEFORE_TAG_CHANGE,
  OBSERVER_COUNTS_TAG_CHANGE,
  OBSERVER_COUNTS_FAVICON_CHANGE,
  OBSERVER_COUNTS_EXTENSIVE_CHANGE_BEGIN,
  OBSERVER_COUNTS_EXTENSIVE_CHANGE_END,
  OBSERVER_COUNTS_BEFORE_REMOVE_ALL,
  OBSERVER_COUNTS_REMOVE_ALL,
  OBSERVER_COUNTS_TOTAL_NUMBER_OF_COUNTS
};

const std::string count_name[] = {
  "OBSERVER_COUNTS_ADD",
  "OBSERVER_COUNTS_BEFORE_REMOVE",
  "OBSERVER_COUNTS_REMOVE",
  "OBSERVER_COUNTS_BEFORE_CHANGE",
  "OBSERVER_COUNTS_CHANGE",
  "OBSERVER_COUNTS_BEFORE_TAG_CHANGE",
  "OBSERVER_COUNTS_TAG_CHANGE",
  "OBSERVER_COUNTS_FAVICON_CHANGE",
  "OBSERVER_COUNTS_EXTENSIVE_CHANGE_BEGIN",
  "OBSERVER_COUNTS_EXTENSIVE_CHANGE_END",
  "OBSERVER_COUNTS_BEFORE_REMOVE_ALL",
  "OBSERVER_COUNTS_REMOVE_ALL"
};


class BookmarkTagModelTest
    : public testing::Test, public BookmarkTagModelObserver {
 public:
  struct ObserverDetails {
    ObserverDetails() : bookmark_(NULL) {}

    void Set(const BookmarkNode* bookmark,
             const std::set<BookmarkTag>& tags) {
      bookmark_ = bookmark;
      tags_ = tags;
    }

    void ExpectEquals(const BookmarkNode* bookmark,
             const std::set<BookmarkTag>& tags) {
      EXPECT_EQ(bookmark_, bookmark);
      EXPECT_EQ(tags_, tags);
    }

   private:
    const BookmarkNode* bookmark_;
    std::set<BookmarkTag> tags_;
  };

  BookmarkTagModelTest() : model_(NULL),
                           tag_model_(new BookmarkTagModel(&model_)) {
    tag_model_->AddObserver(this);
    ClearCounts();
  }

  virtual ~BookmarkTagModelTest() {
  }

  // BookmarkTagModelObserver methods.

  virtual void Loaded(BookmarkTagModel* model) OVERRIDE {
    // We never load from the db, so that this should never get invoked.
    NOTREACHED();
  }

  // Invoked when a node has been added.
  virtual void BookmarkNodeAdded(BookmarkTagModel* model,
                                 const BookmarkNode* bookmark) OVERRIDE {
    ++counts_[OBSERVER_COUNTS_ADD];
    observer_details_.Set(bookmark, model->GetTagsForBookmark(bookmark));
  }

  // Invoked before a node is removed.
  // |node| is the node to be removed.
  virtual void OnWillRemoveBookmarks(BookmarkTagModel* model,
                                     const BookmarkNode* bookmark) OVERRIDE {
    ++counts_[OBSERVER_COUNTS_BEFORE_REMOVE];
  }

  // Invoked when a node has been removed, the item may still be starred though.
  // |node| is the node that was removed.
  virtual void BookmarkNodeRemoved(BookmarkTagModel* model,
                                   const BookmarkNode* bookmark) OVERRIDE {
    ++counts_[OBSERVER_COUNTS_REMOVE];
  }

  // Invoked before the title or url of a node is changed.
  virtual void OnWillChangeBookmarkNode(BookmarkTagModel* model,
                                        const BookmarkNode* bookmark) OVERRIDE {
    ++counts_[OBSERVER_COUNTS_BEFORE_CHANGE];
  }

  // Invoked when the title or url of a node changes.
  virtual void BookmarkNodeChanged(BookmarkTagModel* model,
                                   const BookmarkNode* bookmark) OVERRIDE {
    ++counts_[OBSERVER_COUNTS_CHANGE];
    observer_details_.Set(bookmark, model->GetTagsForBookmark(bookmark));
  }

  virtual void OnWillChangeBookmarkTags(BookmarkTagModel* model,
                                        const BookmarkNode* bookmark) OVERRIDE {
    ++counts_[OBSERVER_COUNTS_BEFORE_TAG_CHANGE];
  }

  virtual void BookmarkTagsChanged(BookmarkTagModel* model,
                                   const BookmarkNode* bookmark) OVERRIDE {
    ++counts_[OBSERVER_COUNTS_TAG_CHANGE];
    observer_details_.Set(bookmark, model->GetTagsForBookmark(bookmark));
  }

  virtual void BookmarkNodeFaviconChanged(BookmarkTagModel* model,
                                          const BookmarkNode* node) OVERRIDE {
    ++counts_[OBSERVER_COUNTS_FAVICON_CHANGE];
  }

  virtual void ExtensiveBookmarkChangesBeginning(BookmarkTagModel* model)
      OVERRIDE {
    ++counts_[OBSERVER_COUNTS_EXTENSIVE_CHANGE_BEGIN];
  }

  virtual void ExtensiveBookmarkChangesEnded(BookmarkTagModel* model) OVERRIDE {
    ++counts_[OBSERVER_COUNTS_EXTENSIVE_CHANGE_END];
  }

  virtual void OnWillRemoveAllBookmarks(BookmarkTagModel* model) OVERRIDE {
    ++counts_[OBSERVER_COUNTS_BEFORE_REMOVE_ALL];
  }

  virtual void BookmarkAllNodesRemoved(BookmarkTagModel* model) OVERRIDE {
    ++counts_[OBSERVER_COUNTS_REMOVE_ALL];
  }

  void ClearCounts() {
    for (unsigned int i = 0; i < OBSERVER_COUNTS_TOTAL_NUMBER_OF_COUNTS; ++i)
      counts_[i] = 0;
  }

  void AssertAndClearObserverCount(ObserverCounts count, int expected) {
    ASSERT_EQ(expected, counts_[count]) << count_name[count];
    counts_[count] = 0;
  }

  void AssertAllCountsClear() {
    for (unsigned int i = 0; i < OBSERVER_COUNTS_TOTAL_NUMBER_OF_COUNTS; ++i)
      ASSERT_EQ(0, counts_[i]) << count_name[i];
  }

  const BookmarkNode* AddURLWithTags(
      const std::string& name,
      const std::set<BookmarkTag>& tags) {
    const string16 title(ASCIIToUTF16(name));
    const GURL url("http://" + name + ".com");

    return tag_model_->AddURL(title, url, tags);
  }

 protected:
  BookmarkModel model_;
  scoped_ptr<BookmarkTagModel> tag_model_;
  ObserverDetails observer_details_;

 private:
  int counts_[OBSERVER_COUNTS_TOTAL_NUMBER_OF_COUNTS];

  DISALLOW_COPY_AND_ASSIGN(BookmarkTagModelTest);
};

TEST_F(BookmarkTagModelTest, InitialState) {
  std::vector<BookmarkTag> tags(tag_model_->TagsRelatedToTag(base::string16()));
  EXPECT_EQ(0UL, tags.size());
}

TEST_F(BookmarkTagModelTest, AddURL) {
  std::set<BookmarkTag> tags;
  tags.insert(ASCIIToUTF16("bar"));
  tags.insert(ASCIIToUTF16("baz"));

  AddURLWithTags("orly", tags);
  const BookmarkNode* new_node = AddURLWithTags("foo", tags);
  AssertAndClearObserverCount(OBSERVER_COUNTS_ADD, 2);
  AssertAllCountsClear();

  observer_details_.ExpectEquals(new_node, tags);

  EXPECT_EQ(2UL, tag_model_->BookmarksForTag(ASCIIToUTF16("bar")).size());
  EXPECT_EQ(2UL, tag_model_->BookmarksForTag(ASCIIToUTF16("baz")).size());
  EXPECT_EQ(tags, tag_model_->GetTagsForBookmark(new_node));

  std::vector<BookmarkTag> alltags(
      tag_model_->TagsRelatedToTag(base::string16()));
  EXPECT_EQ(2UL, alltags.size());
}

TEST_F(BookmarkTagModelTest, RelatedTo) {
  std::set<BookmarkTag> tags;
  tags.insert(ASCIIToUTF16("bar"));
  tags.insert(ASCIIToUTF16("baz"));

  AddURLWithTags("orly", tags);
  AddURLWithTags("foo", tags);
  AssertAndClearObserverCount(OBSERVER_COUNTS_ADD, 2);
  AssertAllCountsClear();

  std::vector<BookmarkTag> bartags(tag_model_->TagsRelatedToTag(
      ASCIIToUTF16("bar")));
  EXPECT_EQ(1UL, bartags.size());
  std::vector<BookmarkTag> baztags(tag_model_->TagsRelatedToTag(
      ASCIIToUTF16("baz")));
  EXPECT_EQ(1UL, baztags.size());
}

TEST_F(BookmarkTagModelTest, AddURLWithWhitespaceTitle) {
  std::set<BookmarkTag> tags;

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(whitespace_test_cases); ++i) {
    const BookmarkNode* new_node = AddURLWithTags(
        whitespace_test_cases[i].input_tag, tags);

    EXPECT_EQ(ASCIIToUTF16(whitespace_test_cases[i].expected_tag),
              new_node->GetTitle());
    EXPECT_EQ(BookmarkNode::URL, new_node->type());
  }
}

TEST_F(BookmarkTagModelTest, CheckTagsWithWhitespace) {
  std::set<BookmarkTag> tags;

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(whitespace_test_cases); ++i)
    tags.insert(ASCIIToUTF16(whitespace_test_cases[i].input_tag));

  AddURLWithTags("foo", tags);

  std::vector<BookmarkTag> alltags(tag_model_->TagsRelatedToTag(
      base::string16()));

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(whitespace_test_cases); ++i) {
    EXPECT_EQ(0UL, tag_model_->BookmarksForTag(
        ASCIIToUTF16(whitespace_test_cases[i].input_tag)).size());
    EXPECT_EQ(1UL, tag_model_->BookmarksForTag(
        ASCIIToUTF16(whitespace_test_cases[i].expected_tag)).size());
  }
}

TEST_F(BookmarkTagModelTest, RemoveURL) {
  std::set<BookmarkTag> tags;
  tags.insert(ASCIIToUTF16("bar"));
  tags.insert(ASCIIToUTF16("baz"));

  const BookmarkNode* new_node = AddURLWithTags("foo", tags);
  const BookmarkNode* new_node2 = AddURLWithTags("flou", tags);
  ClearCounts();

  tag_model_->Remove(new_node);
  tag_model_->Remove(new_node2);
  AssertAndClearObserverCount(OBSERVER_COUNTS_BEFORE_REMOVE, 2);
  AssertAndClearObserverCount(OBSERVER_COUNTS_REMOVE, 2);
  AssertAllCountsClear();

  std::vector<BookmarkTag> alltags(tag_model_->TagsRelatedToTag(
      base::string16()));
  EXPECT_EQ(0UL, alltags.size());
}

TEST_F(BookmarkTagModelTest, AddTagToBookmarks) {
  std::set<BookmarkTag> tags;
  tags.insert(ASCIIToUTF16("bar"));
  tags.insert(ASCIIToUTF16("baz"));

  std::set<const BookmarkNode*> bookmarks;
  bookmarks.insert(AddURLWithTags("foo", tags));
  bookmarks.insert(AddURLWithTags("flou", tags));
  ClearCounts();

  std::set<BookmarkTag> new_tags;
  new_tags.insert(ASCIIToUTF16("new_bar"));
  new_tags.insert(ASCIIToUTF16("new_baz"));

  tag_model_->AddTagsToBookmarks(new_tags, bookmarks);
  AssertAndClearObserverCount(OBSERVER_COUNTS_BEFORE_TAG_CHANGE, 2);
  AssertAndClearObserverCount(OBSERVER_COUNTS_TAG_CHANGE, 2);
  AssertAllCountsClear();

  std::vector<BookmarkTag> alltags(tag_model_->TagsRelatedToTag(
      base::string16()));
  EXPECT_EQ(4UL, alltags.size());
}

TEST_F(BookmarkTagModelTest, AddTagToBookmark) {
  std::set<BookmarkTag> tags;
  tags.insert(ASCIIToUTF16("bar"));
  tags.insert(ASCIIToUTF16("baz"));

  const BookmarkNode* bookmark = AddURLWithTags("foo", tags);
  ClearCounts();

  std::set<BookmarkTag> new_tags;
  new_tags.insert(ASCIIToUTF16("new_bar"));
  new_tags.insert(ASCIIToUTF16("new_baz"));

  tag_model_->AddTagsToBookmark(new_tags, bookmark);
  AssertAndClearObserverCount(OBSERVER_COUNTS_BEFORE_TAG_CHANGE, 1);
  AssertAndClearObserverCount(OBSERVER_COUNTS_TAG_CHANGE, 1);
  AssertAllCountsClear();

  std::vector<BookmarkTag> alltags(tag_model_->TagsRelatedToTag(
      base::string16()));
  EXPECT_EQ(4UL, alltags.size());
}

TEST_F(BookmarkTagModelTest, RemoveTagFromBookmarks) {
  std::set<BookmarkTag> tags;
  tags.insert(ASCIIToUTF16("bar"));
  tags.insert(ASCIIToUTF16("baz"));

  std::set<const BookmarkNode*> bookmarks;

  bookmarks.insert(AddURLWithTags("foo", tags));
  bookmarks.insert(AddURLWithTags("flou", tags));
  ClearCounts();

  tag_model_->RemoveTagsFromBookmarks(tags, bookmarks);
  AssertAndClearObserverCount(OBSERVER_COUNTS_BEFORE_TAG_CHANGE, 2);
  AssertAndClearObserverCount(OBSERVER_COUNTS_TAG_CHANGE, 2);
  AssertAllCountsClear();

  std::vector<BookmarkTag> alltags(tag_model_->TagsRelatedToTag(
      base::string16()));
  EXPECT_EQ(0UL, alltags.size());
}

TEST_F(BookmarkTagModelTest, RemoveTagFromBookmark) {
  std::set<BookmarkTag> tags;
  tags.insert(ASCIIToUTF16("bar"));
  tags.insert(ASCIIToUTF16("baz"));

  const BookmarkNode* bookmark = AddURLWithTags("foo", tags);
  ClearCounts();

  tag_model_->RemoveTagsFromBookmark(tags, bookmark);
  AssertAndClearObserverCount(OBSERVER_COUNTS_BEFORE_TAG_CHANGE, 1);
  AssertAndClearObserverCount(OBSERVER_COUNTS_TAG_CHANGE, 1);
  AssertAllCountsClear();

  std::vector<BookmarkTag> alltags(tag_model_->TagsRelatedToTag(
      base::string16()));
  EXPECT_EQ(0UL, alltags.size());
}

TEST_F(BookmarkTagModelTest, RemoveAll) {
  std::set<BookmarkTag> tags;
  tags.insert(ASCIIToUTF16("bar"));
  tags.insert(ASCIIToUTF16("baz"));

  AddURLWithTags("foo", tags);
  ClearCounts();

  model_.RemoveAll();
  AssertAndClearObserverCount(OBSERVER_COUNTS_EXTENSIVE_CHANGE_BEGIN, 1);
  AssertAndClearObserverCount(OBSERVER_COUNTS_BEFORE_REMOVE_ALL, 1);
  AssertAndClearObserverCount(OBSERVER_COUNTS_REMOVE_ALL, 1);
  AssertAndClearObserverCount(OBSERVER_COUNTS_EXTENSIVE_CHANGE_END, 1);
  AssertAllCountsClear();

  std::vector<BookmarkTag> alltags(tag_model_->TagsRelatedToTag(
      base::string16()));
  EXPECT_EQ(0UL, alltags.size());
}

TEST_F(BookmarkTagModelTest, DuplicateFolders) {
  const BookmarkNode* left = model_.AddFolder(model_.bookmark_bar_node(), 0,
                                              ASCIIToUTF16("left"));
  const BookmarkNode* right = model_.AddFolder(model_.bookmark_bar_node(), 0,
                                               ASCIIToUTF16("right"));
  const BookmarkNode* left_handed = model_.AddFolder(left, 0,
                                                     ASCIIToUTF16("handed"));
  const BookmarkNode* right_handed = model_.AddFolder(right, 0,
                                                     ASCIIToUTF16("handed"));
  model_.AddURL(
      left_handed, 0, ASCIIToUTF16("red"), GURL("http://random.com"));
  model_.AddURL(
      right_handed, 0, ASCIIToUTF16("der"), GURL("http://random.com"));

  EXPECT_EQ(2UL, tag_model_->BookmarksForTag(ASCIIToUTF16("handed")).size());
  EXPECT_EQ(1UL, tag_model_->BookmarksForTag(ASCIIToUTF16("left")).size());
  EXPECT_EQ(1UL, tag_model_->BookmarksForTag(ASCIIToUTF16("right")).size());
  std::set<BookmarkTag> tags;
  tags.insert(ASCIIToUTF16("left"));
  tags.insert(ASCIIToUTF16("handed"));
  EXPECT_EQ(1UL, tag_model_->BookmarksForTags(tags).size());
}

class PreloadedBookmarkTagModelTest : public BookmarkTagModelTest {
 public:
  PreloadedBookmarkTagModelTest() : BookmarkTagModelTest() {
    PopulateUnderlyingModel();
  }

  void PopulateUnderlyingModel() {
    ClearCounts();
    top_node_ = model_.AddURL(model_.bookmark_bar_node(), 0,
        ASCIIToUTF16("Tagless"), GURL("http://example.com"));
    folder_1_ = model_.AddFolder(model_.bookmark_bar_node(), 0,
                                 ASCIIToUTF16("folder1"));
    one_tag_ = model_.AddURL(folder_1_, 0, ASCIIToUTF16("OneTag"),
                             GURL("http://random.com"));
    folder_2_ = model_.AddFolder(folder_1_, 0, ASCIIToUTF16("folder2"));
    two_tags_ = model_.AddURL(folder_2_, 0, ASCIIToUTF16("TwoTags"),
                              GURL("http://moveit.com"));
    AssertAndClearObserverCount(OBSERVER_COUNTS_ADD, 3);
    AssertAllCountsClear();
  }

  void AssertModelMatches() {
    EXPECT_EQ(2UL, tag_model_->BookmarksForTag(ASCIIToUTF16("folder1")).size());
    EXPECT_EQ(1UL, tag_model_->BookmarksForTag(ASCIIToUTF16("folder2")).size());

    std::set<BookmarkTag> tags;
    tags.insert(ASCIIToUTF16("folder1"));
    EXPECT_EQ(tags, tag_model_->GetTagsForBookmark(one_tag_));

    tags.insert(ASCIIToUTF16("folder2"));
    const BookmarkNode* two_tags = tag_model_->
        GetMostRecentlyAddedBookmarkForURL(GURL("http://moveit.com"));
    EXPECT_EQ(tags, tag_model_->GetTagsForBookmark(two_tags));

    std::vector<BookmarkTag> alltags(tag_model_->TagsRelatedToTag(
        base::string16()));
    EXPECT_EQ(2UL, alltags.size());
  }

 protected:
  const BookmarkNode* folder_1_;
  const BookmarkNode* folder_2_;
  const BookmarkNode* top_node_;
  const BookmarkNode* one_tag_;
  const BookmarkNode* two_tags_;

  DISALLOW_COPY_AND_ASSIGN(PreloadedBookmarkTagModelTest);
};

TEST_F(PreloadedBookmarkTagModelTest, InitialState) {
  AssertAllCountsClear();
  AssertModelMatches();
}

TEST_F(PreloadedBookmarkTagModelTest, FromExistingState) {
  tag_model_.reset(new BookmarkTagModel(&model_));
  tag_model_->AddObserver(this);
  AssertAllCountsClear();
  AssertModelMatches();
}

TEST_F(PreloadedBookmarkTagModelTest, BookmarkChange) {
  AssertAllCountsClear();
  tag_model_->SetTitle(top_node_, ASCIIToUTF16("newname"));
  AssertAndClearObserverCount(OBSERVER_COUNTS_BEFORE_CHANGE, 1);
  AssertAndClearObserverCount(OBSERVER_COUNTS_CHANGE, 1);
  AssertAllCountsClear();
}

TEST_F(PreloadedBookmarkTagModelTest, UnchangedBookmarkMove) {
  AssertAllCountsClear();
  model_.Move(top_node_, folder_2_, 0);
  AssertAndClearObserverCount(OBSERVER_COUNTS_BEFORE_TAG_CHANGE, 1);
  AssertAndClearObserverCount(OBSERVER_COUNTS_TAG_CHANGE, 1);
  AssertAllCountsClear();
}

TEST_F(PreloadedBookmarkTagModelTest, ChangedBookmarkMove) {
  std::set<BookmarkTag> tags;
  tags.insert(ASCIIToUTF16("bar"));

  AssertAllCountsClear();
  tag_model_->AddTagsToBookmark(tags, top_node_);
  AssertAndClearObserverCount(OBSERVER_COUNTS_BEFORE_TAG_CHANGE, 1);
  AssertAndClearObserverCount(OBSERVER_COUNTS_TAG_CHANGE, 1);
  AssertAllCountsClear();

  model_.Move(top_node_, folder_2_, 0);
  AssertAllCountsClear();
}

TEST_F(PreloadedBookmarkTagModelTest, DuplicateBookmark) {
  EXPECT_EQ(2UL, tag_model_->BookmarksForTag(folder_1_->GetTitle()).size());
  model_.AddURL(folder_1_, 0, one_tag_->GetTitle(), one_tag_->url());
  EXPECT_EQ(3UL, tag_model_->BookmarksForTag(folder_1_->GetTitle()).size());
}

TEST_F(PreloadedBookmarkTagModelTest, NamelessFolders) {
  const BookmarkNode* folder = model_.AddFolder(model_.bookmark_bar_node(), 0,
                                                ASCIIToUTF16(""));
  model_.AddURL(folder, 0, ASCIIToUTF16("StillNotag"),
                GURL("http://random.com"));
  AssertModelMatches();
}

TEST_F(PreloadedBookmarkTagModelTest, FolderNameChange) {
  EXPECT_EQ(2UL, tag_model_->BookmarksForTag(folder_1_->GetTitle()).size());
  model_.SetTitle(folder_1_, ASCIIToUTF16("Bummer"));
  AssertAndClearObserverCount(OBSERVER_COUNTS_EXTENSIVE_CHANGE_BEGIN, 1);
  AssertAndClearObserverCount(OBSERVER_COUNTS_EXTENSIVE_CHANGE_END, 1);
  AssertAndClearObserverCount(OBSERVER_COUNTS_BEFORE_TAG_CHANGE, 2);
  AssertAndClearObserverCount(OBSERVER_COUNTS_TAG_CHANGE, 2);
  AssertAllCountsClear();

  EXPECT_EQ(2UL, tag_model_->BookmarksForTag(folder_1_->GetTitle()).size());
}

TEST_F(PreloadedBookmarkTagModelTest, FolderMoved) {
  EXPECT_EQ(2UL, tag_model_->BookmarksForTag(folder_1_->GetTitle()).size());
  model_.Move(folder_2_, model_.bookmark_bar_node(), 0);
  AssertAndClearObserverCount(OBSERVER_COUNTS_EXTENSIVE_CHANGE_BEGIN, 1);
  AssertAndClearObserverCount(OBSERVER_COUNTS_EXTENSIVE_CHANGE_END, 1);
  AssertAndClearObserverCount(OBSERVER_COUNTS_BEFORE_TAG_CHANGE, 1);
  AssertAndClearObserverCount(OBSERVER_COUNTS_TAG_CHANGE, 1);
  AssertAllCountsClear();

  EXPECT_EQ(1UL, tag_model_->BookmarksForTag(folder_1_->GetTitle()).size());
}

}  // namespace
