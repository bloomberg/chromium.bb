// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_editor_view.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

// Base class for bookmark editor tests. Creates a BookmarkModel and populates
// it with test data.
class BookmarkEditorViewTest : public testing::Test {
 public:
  BookmarkEditorViewTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_),
        model_(NULL) {
  }

  virtual void SetUp() {
    profile_.reset(new TestingProfile());
    profile_->set_has_history_service(true);
    profile_->CreateBookmarkModel(true);

    model_ = profile_->GetBookmarkModel();
    profile_->BlockUntilBookmarkModelLoaded();

    AddTestData();
  }

  virtual void TearDown() {
  }

 protected:
  std::string base_path() const { return "file:///c:/tmp/"; }

  const BookmarkNode* GetNode(const std::string& name) {
    return model_->GetMostRecentlyAddedNodeForURL(GURL(base_path() + name));
  }

  BookmarkNode* GetMutableNode(const std::string& name) {
    return const_cast<BookmarkNode*>(GetNode(name));
  }

  BookmarkEditorView::EditorTreeModel* editor_tree_model() {
    return editor_->tree_model_.get();
  }

  void CreateEditor(Profile* profile,
                    const BookmarkNode* parent,
                    const BookmarkEditor::EditDetails& details,
                    BookmarkEditor::Configuration configuration) {
    editor_.reset(new BookmarkEditorView(profile, parent, details,
                                         configuration));
  }

  void SetTitleText(const std::wstring& title) {
    editor_->title_tf_.SetText(title);
  }

  void SetURLText(const std::wstring& text) {
    editor_->url_tf_.SetText(text);
  }

  void ApplyEdits(BookmarkEditorView::EditorNode* node) {
    editor_->ApplyEdits(node);
  }

  BookmarkEditorView::EditorNode* AddNewGroup(
      BookmarkEditorView::EditorNode* parent) {
    return editor_->AddNewGroup(parent);
  }

  bool URLTFHasParent() {
    return editor_->url_tf_.parent();
  }

  MessageLoopForUI message_loop_;
  BrowserThread ui_thread_;
  BrowserThread file_thread_;

  BookmarkModel* model_;
  scoped_ptr<TestingProfile> profile_;

 private:
  // Creates the following structure:
  // bookmark bar node
  //   a
  //   F1
  //    f1a
  //    F11
  //     f11a
  //   F2
  // other node
  //   oa
  //   OF1
  //     of1a
  void AddTestData() {
    std::string test_base = base_path();

    model_->AddURL(model_->GetBookmarkBarNode(), 0, ASCIIToUTF16("a"),
                   GURL(test_base + "a"));
    const BookmarkNode* f1 =
        model_->AddGroup(model_->GetBookmarkBarNode(), 1, ASCIIToUTF16("F1"));
    model_->AddURL(f1, 0, ASCIIToUTF16("f1a"), GURL(test_base + "f1a"));
    const BookmarkNode* f11 = model_->AddGroup(f1, 1, ASCIIToUTF16("F11"));
    model_->AddURL(f11, 0, ASCIIToUTF16("f11a"), GURL(test_base + "f11a"));
    model_->AddGroup(model_->GetBookmarkBarNode(), 2, ASCIIToUTF16("F2"));

    // Children of the other node.
    model_->AddURL(model_->other_node(), 0, ASCIIToUTF16("oa"),
                   GURL(test_base + "oa"));
    const BookmarkNode* of1 =
        model_->AddGroup(model_->other_node(), 1, ASCIIToUTF16("OF1"));
    model_->AddURL(of1, 0, ASCIIToUTF16("of1a"), GURL(test_base + "of1a"));
  }

  scoped_ptr<BookmarkEditorView> editor_;
};

// Makes sure the tree model matches that of the bookmark bar model.
TEST_F(BookmarkEditorViewTest, ModelsMatch) {
  CreateEditor(profile_.get(), NULL, BookmarkEditor::EditDetails(),
               BookmarkEditorView::SHOW_TREE);
  BookmarkEditorView::EditorNode* editor_root = editor_tree_model()->GetRoot();
  // The root should have two children, one for the bookmark bar node,
  // the other for the 'other bookmarks' folder.
  ASSERT_EQ(2, editor_root->child_count());

  BookmarkEditorView::EditorNode* bb_node = editor_root->GetChild(0);
  // The root should have 2 nodes: folder F1 and F2.
  ASSERT_EQ(2, bb_node->child_count());
  ASSERT_EQ(ASCIIToUTF16("F1"), bb_node->GetChild(0)->GetTitle());
  ASSERT_EQ(ASCIIToUTF16("F2"), bb_node->GetChild(1)->GetTitle());

  // F1 should have one child, F11
  ASSERT_EQ(1, bb_node->GetChild(0)->child_count());
  ASSERT_EQ(ASCIIToUTF16("F11"), bb_node->GetChild(0)->GetChild(0)->GetTitle());

  BookmarkEditorView::EditorNode* other_node = editor_root->GetChild(1);
  // Other node should have one child (OF1).
  ASSERT_EQ(1, other_node->child_count());
  ASSERT_EQ(ASCIIToUTF16("OF1"), other_node->GetChild(0)->GetTitle());
}

// Changes the title and makes sure parent/visual order doesn't change.
TEST_F(BookmarkEditorViewTest, EditTitleKeepsPosition) {
  CreateEditor(profile_.get(), NULL, BookmarkEditor::EditDetails(GetNode("a")),
               BookmarkEditorView::SHOW_TREE);
  SetTitleText(L"new_a");

  ApplyEdits(editor_tree_model()->GetRoot()->GetChild(0));

  const BookmarkNode* bb_node =
      profile_->GetBookmarkModel()->GetBookmarkBarNode();
  ASSERT_EQ(ASCIIToUTF16("new_a"), bb_node->GetChild(0)->GetTitle());
  // The URL shouldn't have changed.
  ASSERT_TRUE(GURL(base_path() + "a") == bb_node->GetChild(0)->GetURL());
}

// Changes the url and makes sure parent/visual order doesn't change.
TEST_F(BookmarkEditorViewTest, EditURLKeepsPosition) {
  Time node_time = Time::Now() + TimeDelta::FromDays(2);
  GetMutableNode("a")->set_date_added(node_time);
  CreateEditor(profile_.get(), NULL, BookmarkEditor::EditDetails(GetNode("a")),
               BookmarkEditorView::SHOW_TREE);

  SetURLText(UTF8ToWide(GURL(base_path() + "new_a").spec()));

  ApplyEdits(editor_tree_model()->GetRoot()->GetChild(0));

  const BookmarkNode* bb_node =
      profile_->GetBookmarkModel()->GetBookmarkBarNode();
  ASSERT_EQ(ASCIIToUTF16("a"), bb_node->GetChild(0)->GetTitle());
  // The URL should have changed.
  ASSERT_TRUE(GURL(base_path() + "new_a") == bb_node->GetChild(0)->GetURL());
  ASSERT_TRUE(node_time == bb_node->GetChild(0)->date_added());
}

// Moves 'a' to be a child of the other node.
TEST_F(BookmarkEditorViewTest, ChangeParent) {
  CreateEditor(profile_.get(), NULL, BookmarkEditor::EditDetails(GetNode("a")),
               BookmarkEditorView::SHOW_TREE);

  ApplyEdits(editor_tree_model()->GetRoot()->GetChild(1));

  const BookmarkNode* other_node = profile_->GetBookmarkModel()->other_node();
  ASSERT_EQ(ASCIIToUTF16("a"), other_node->GetChild(2)->GetTitle());
  ASSERT_TRUE(GURL(base_path() + "a") == other_node->GetChild(2)->GetURL());
}

// Moves 'a' to be a child of the other node and changes its url to new_a.
TEST_F(BookmarkEditorViewTest, ChangeParentAndURL) {
  Time node_time = Time::Now() + TimeDelta::FromDays(2);
  GetMutableNode("a")->set_date_added(node_time);
  CreateEditor(profile_.get(), NULL, BookmarkEditor::EditDetails(GetNode("a")),
               BookmarkEditorView::SHOW_TREE);

  SetURLText(UTF8ToWide(GURL(base_path() + "new_a").spec()));

  ApplyEdits(editor_tree_model()->GetRoot()->GetChild(1));

  const BookmarkNode* other_node = profile_->GetBookmarkModel()->other_node();
  ASSERT_EQ(ASCIIToUTF16("a"), other_node->GetChild(2)->GetTitle());
  ASSERT_TRUE(GURL(base_path() + "new_a") == other_node->GetChild(2)->GetURL());
  ASSERT_TRUE(node_time == other_node->GetChild(2)->date_added());
}

// Creates a new folder and moves a node to it.
TEST_F(BookmarkEditorViewTest, MoveToNewParent) {
  CreateEditor(profile_.get(), NULL, BookmarkEditor::EditDetails(GetNode("a")),
               BookmarkEditorView::SHOW_TREE);

  // Create two nodes: "F21" as a child of "F2" and "F211" as a child of "F21".
  BookmarkEditorView::EditorNode* f2 =
      editor_tree_model()->GetRoot()->GetChild(0)->GetChild(1);
  BookmarkEditorView::EditorNode* f21 = AddNewGroup(f2);
  f21->set_title(ASCIIToUTF16("F21"));
  BookmarkEditorView::EditorNode* f211 = AddNewGroup(f21);
  f211->set_title(ASCIIToUTF16("F211"));

  // Parent the node to "F21".
  ApplyEdits(f2);

  const BookmarkNode* bb_node =
      profile_->GetBookmarkModel()->GetBookmarkBarNode();
  const BookmarkNode* mf2 = bb_node->GetChild(1);

  // F2 in the model should have two children now: F21 and the node edited.
  ASSERT_EQ(2, mf2->child_count());
  // F21 should be first.
  ASSERT_EQ(ASCIIToUTF16("F21"), mf2->GetChild(0)->GetTitle());
  // Then a.
  ASSERT_EQ(ASCIIToUTF16("a"), mf2->GetChild(1)->GetTitle());

  // F21 should have one child, F211.
  const BookmarkNode* mf21 = mf2->GetChild(0);
  ASSERT_EQ(1, mf21->child_count());
  ASSERT_EQ(ASCIIToUTF16("F211"), mf21->GetChild(0)->GetTitle());
}

// Brings up the editor, creating a new URL on the bookmark bar.
TEST_F(BookmarkEditorViewTest, NewURL) {
  CreateEditor(profile_.get(), NULL, BookmarkEditor::EditDetails(),
               BookmarkEditorView::SHOW_TREE);

  SetURLText(UTF8ToWide(GURL(base_path() + "a").spec()));
  SetTitleText(L"new_a");

  ApplyEdits(editor_tree_model()->GetRoot()->GetChild(0));

  const BookmarkNode* bb_node =
      profile_->GetBookmarkModel()->GetBookmarkBarNode();
  ASSERT_EQ(4, bb_node->child_count());

  const BookmarkNode* new_node = bb_node->GetChild(3);

  EXPECT_EQ(ASCIIToUTF16("new_a"), new_node->GetTitle());
  EXPECT_TRUE(GURL(base_path() + "a") == new_node->GetURL());
}

// Brings up the editor with no tree and modifies the url.
TEST_F(BookmarkEditorViewTest, ChangeURLNoTree) {
  CreateEditor(profile_.get(), NULL,
               BookmarkEditor::EditDetails(model_->other_node()->GetChild(0)),
               BookmarkEditorView::NO_TREE);

  SetURLText(UTF8ToWide(GURL(base_path() + "a").spec()));
  SetTitleText(L"new_a");

  ApplyEdits(NULL);

  const BookmarkNode* other_node = profile_->GetBookmarkModel()->other_node();
  ASSERT_EQ(2, other_node->child_count());

  const BookmarkNode* new_node = other_node->GetChild(0);

  EXPECT_EQ(ASCIIToUTF16("new_a"), new_node->GetTitle());
  EXPECT_TRUE(GURL(base_path() + "a") == new_node->GetURL());
}

// Brings up the editor with no tree and modifies only the title.
TEST_F(BookmarkEditorViewTest, ChangeTitleNoTree) {
  CreateEditor(profile_.get(), NULL,
               BookmarkEditor::EditDetails(model_->other_node()->GetChild(0)),
               BookmarkEditorView::NO_TREE);

  SetTitleText(L"new_a");

  ApplyEdits(NULL);

  const BookmarkNode* other_node = profile_->GetBookmarkModel()->other_node();
  ASSERT_EQ(2, other_node->child_count());

  const BookmarkNode* new_node = other_node->GetChild(0);

  EXPECT_EQ(ASCIIToUTF16("new_a"), new_node->GetTitle());
}

// Creates a new folder.
TEST_F(BookmarkEditorViewTest, NewFolder) {
  BookmarkEditor::EditDetails details;
  details.urls.push_back(std::make_pair(GURL(base_path() + "x"),
                                        ASCIIToUTF16("z")));
  details.type = BookmarkEditor::EditDetails::NEW_FOLDER;
  CreateEditor(profile_.get(), model_->GetBookmarkBarNode(),
               details, BookmarkEditorView::SHOW_TREE);

  // The url field shouldn't be visible.
  EXPECT_FALSE(URLTFHasParent());
  SetTitleText(L"new_F");

  ApplyEdits(editor_tree_model()->GetRoot()->GetChild(0));

  // Make sure the folder was created.
  ASSERT_EQ(4, model_->GetBookmarkBarNode()->child_count());
  const BookmarkNode* new_node =
      model_->GetBookmarkBarNode()->GetChild(3);
  EXPECT_EQ(BookmarkNode::FOLDER, new_node->type());
  EXPECT_EQ(ASCIIToUTF16("new_F"), new_node->GetTitle());
  // The node should have one child.
  ASSERT_EQ(1, new_node->child_count());
  const BookmarkNode* new_child = new_node->GetChild(0);
  // Make sure the child url/title match.
  EXPECT_EQ(BookmarkNode::URL, new_child->type());
  EXPECT_EQ(WideToUTF16Hack(details.urls[0].second), new_child->GetTitle());
  EXPECT_EQ(details.urls[0].first, new_child->GetURL());
}

// Creates a new folder and selects a different folder for the folder to appear
// in then the editor is initially created showing.
TEST_F(BookmarkEditorViewTest, MoveFolder) {
  BookmarkEditor::EditDetails details;
  details.urls.push_back(std::make_pair(GURL(base_path() + "x"),
                                        ASCIIToUTF16("z")));
  details.type = BookmarkEditor::EditDetails::NEW_FOLDER;
  CreateEditor(profile_.get(), model_->GetBookmarkBarNode(),
               details, BookmarkEditorView::SHOW_TREE);

  SetTitleText(L"new_F");

  // Create the folder in the 'other' folder.
  ApplyEdits(editor_tree_model()->GetRoot()->GetChild(1));

  // Make sure the folder we edited is still there.
  ASSERT_EQ(3, model_->other_node()->child_count());
  const BookmarkNode* new_node = model_->other_node()->GetChild(2);
  EXPECT_EQ(BookmarkNode::FOLDER, new_node->type());
  EXPECT_EQ(ASCIIToUTF16("new_F"), new_node->GetTitle());
  // The node should have one child.
  ASSERT_EQ(1, new_node->child_count());
  const BookmarkNode* new_child = new_node->GetChild(0);
  // Make sure the child url/title match.
  EXPECT_EQ(BookmarkNode::URL, new_child->type());
  EXPECT_EQ(WideToUTF16Hack(details.urls[0].second), new_child->GetTitle());
  EXPECT_EQ(details.urls[0].first, new_child->GetURL());
}
