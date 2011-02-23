// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/keyword_editor_view.h"

#include <gtk/gtk.h>

#include <string>
#include <vector>

#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/ui/gtk/gtk_tree.h"
#include "chrome/browser/ui/search_engines/template_url_table_model.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

class KeywordEditorViewTest : public testing::Test {
 public:
  virtual void SetUp() {
    profile_.reset(new TestingProfile());
    profile_->CreateTemplateURLModel();
  }

  TemplateURL* AddToModel(const std::string& name,
                          const std::string& keyword,
                          bool make_default) {
    TemplateURL* template_url = new TemplateURL();
    template_url->set_short_name(UTF8ToUTF16(name));
    template_url->set_keyword(UTF8ToUTF16(keyword));
    template_url->SetURL("http://example.com/{searchTerms}", 0, 0);
    profile_->GetTemplateURLModel()->Add(template_url);
    if (make_default)
      profile_->GetTemplateURLModel()->SetDefaultSearchProvider(template_url);
    return template_url;
  }

  int GetSelectedRowNum(const KeywordEditorView& editor) {
    GtkTreeIter iter;
    if (!gtk_tree_selection_get_selected(editor.selection_, NULL, &iter))
      return -1;
    return gtk_tree::GetRowNumForIter(GTK_TREE_MODEL(editor.list_store_),
                                      &iter);
  }

  // Get the search engines displayed in the dialog in the order they are
  // displayed, as a comma seperated string.
  // The headers are included as "!,_" for the first group header and "_,@,_"
  // for the second group header (This allows the tests to ensure the headers
  // aren't accidentally misplaced/moved.)
  // Ex: EXPECT_STREQ("!,_,A (Default),_,@,_,B",
  //                  GetDisplayedEngines(editor).c_str());
  std::string GetDisplayedEngines(const KeywordEditorView& editor) {
    ui::TableModel::Groups groups(editor.table_model_->GetGroups());
    std::vector<std::string> parts;
    GtkTreeModel* tree_model = GTK_TREE_MODEL(editor.list_store_);
    GtkTreeIter iter;
    if (!gtk_tree_model_get_iter_first(tree_model, &iter))
      return std::string();
    while (true) {
      gchar* name;
      gboolean is_header;
      gtk_tree_model_get(tree_model, &iter,
                         KeywordEditorView::COL_TITLE, &name,
                         KeywordEditorView::COL_IS_HEADER, &is_header,
                         -1);
      if (name && UTF16ToUTF8(groups[0].title) == name)
        parts.push_back("!");
      else if (name && UTF16ToUTF8(groups[1].title) == name)
        parts.push_back("@");
      else if (is_header)
        parts.push_back("_");
      else if (name)
        parts.push_back(name);
      else
        parts.push_back("???");
      if (name)
        g_free(name);
      if (!gtk_tree_model_iter_next(tree_model, &iter))
        break;
    }
    return JoinString(parts, ',');
  }

 protected:
  MessageLoopForUI message_loop_;
  scoped_ptr<TestingProfile> profile_;
};

TEST_F(KeywordEditorViewTest, Empty) {
  KeywordEditorView editor(profile_.get());
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(editor.add_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(editor.edit_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(editor.remove_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(editor.make_default_button_));
  EXPECT_STREQ("!,_,_,@,_", GetDisplayedEngines(editor).c_str());
  EXPECT_EQ(-1, GetSelectedRowNum(editor));
}

TEST_F(KeywordEditorViewTest, Add) {
  AddToModel("A1", "k1", true);
  KeywordEditorView editor(profile_.get());
  EXPECT_STREQ("!,_,A1 (Default),_,@,_", GetDisplayedEngines(editor).c_str());
  EXPECT_EQ(-1, GetSelectedRowNum(editor));

  editor.OnEditedKeyword(NULL, ASCIIToUTF16("B"), ASCIIToUTF16("b"),
                         "example.com");
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(editor.add_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(editor.edit_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(editor.remove_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(editor.make_default_button_));
  EXPECT_STREQ("!,_,A1 (Default),_,@,_,B", GetDisplayedEngines(editor).c_str());
  EXPECT_EQ(6, GetSelectedRowNum(editor));

  editor.OnEditedKeyword(NULL, ASCIIToUTF16("C"), ASCIIToUTF16("c"),
                         "example.com/{searchTerms}");
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(editor.add_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(editor.edit_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(editor.remove_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(editor.make_default_button_));
  EXPECT_STREQ("!,_,A1 (Default),_,@,_,B,C",
               GetDisplayedEngines(editor).c_str());
  EXPECT_EQ(7, GetSelectedRowNum(editor));

  editor.OnEditedKeyword(NULL, ASCIIToUTF16("D"), ASCIIToUTF16("d"),
                         "example.com");
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(editor.add_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(editor.edit_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(editor.remove_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(editor.make_default_button_));
  EXPECT_STREQ("!,_,A1 (Default),_,@,_,B,C,D",
               GetDisplayedEngines(editor).c_str());
  EXPECT_EQ(8, GetSelectedRowNum(editor));
}

TEST_F(KeywordEditorViewTest, MakeDefault) {
  AddToModel("A", "a", true);
  AddToModel("B", "b", false);
  AddToModel("C", "c", false);
  AddToModel("D", "d", false);
  KeywordEditorView editor(profile_.get());
  EXPECT_STREQ("!,_,A (Default),_,@,_,B,C,D",
               GetDisplayedEngines(editor).c_str());

  GtkTreeIter iter;
  gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(editor.list_store_),
                                &iter, NULL, 6);
  gtk_tree_selection_select_iter(editor.selection_, &iter);
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(editor.make_default_button_));

  gtk_button_clicked(GTK_BUTTON(editor.make_default_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(editor.remove_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(editor.make_default_button_));
  EXPECT_STREQ("!,_,A,B (Default),_,@,_,C,D",
               GetDisplayedEngines(editor).c_str());
  EXPECT_EQ(3, GetSelectedRowNum(editor));

  gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(editor.list_store_),
                                &iter, NULL, 8);
  gtk_tree_selection_select_iter(editor.selection_, &iter);
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(editor.make_default_button_));

  gtk_button_clicked(GTK_BUTTON(editor.make_default_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(editor.remove_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(editor.make_default_button_));
  EXPECT_STREQ("!,_,A,B,D (Default),_,@,_,C",
               GetDisplayedEngines(editor).c_str());
  EXPECT_EQ(4, GetSelectedRowNum(editor));

  gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(editor.list_store_),
                                &iter, NULL, 2);
  gtk_tree_selection_select_iter(editor.selection_, &iter);
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(editor.make_default_button_));

  gtk_button_clicked(GTK_BUTTON(editor.make_default_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(editor.remove_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(editor.make_default_button_));
  EXPECT_STREQ("!,_,A (Default),B,D,_,@,_,C",
               GetDisplayedEngines(editor).c_str());
  EXPECT_EQ(2, GetSelectedRowNum(editor));

  gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(editor.list_store_),
                                &iter, NULL, 4);
  gtk_tree_selection_select_iter(editor.selection_, &iter);
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(editor.make_default_button_));

  gtk_button_clicked(GTK_BUTTON(editor.make_default_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(editor.remove_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(editor.make_default_button_));
  EXPECT_STREQ("!,_,A,B,D (Default),_,@,_,C",
               GetDisplayedEngines(editor).c_str());
  EXPECT_EQ(4, GetSelectedRowNum(editor));
}

TEST_F(KeywordEditorViewTest, Remove) {
  AddToModel("A", "a", true);
  AddToModel("B", "b", true);
  AddToModel("C", "c", false);
  AddToModel("D", "d", false);
  KeywordEditorView editor(profile_.get());
  EXPECT_STREQ("!,_,A,B (Default),_,@,_,C,D",
               GetDisplayedEngines(editor).c_str());

  GtkTreeIter iter;
  gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(editor.list_store_),
                                &iter, NULL, 2);
  gtk_tree_selection_select_iter(editor.selection_, &iter);
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(editor.remove_button_));

  gtk_button_clicked(GTK_BUTTON(editor.remove_button_));
  EXPECT_STREQ("!,_,B (Default),_,@,_,C,D",
               GetDisplayedEngines(editor).c_str());
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(editor.remove_button_));
  EXPECT_EQ(2, GetSelectedRowNum(editor));

  gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(editor.list_store_),
                                &iter, NULL, 6);
  gtk_tree_selection_select_iter(editor.selection_, &iter);
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(editor.remove_button_));

  gtk_button_clicked(GTK_BUTTON(editor.remove_button_));
  EXPECT_STREQ("!,_,B (Default),_,@,_,D",
               GetDisplayedEngines(editor).c_str());
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(editor.remove_button_));
  EXPECT_EQ(6, GetSelectedRowNum(editor));

  gtk_button_clicked(GTK_BUTTON(editor.remove_button_));
  EXPECT_STREQ("!,_,B (Default),_,@,_",
               GetDisplayedEngines(editor).c_str());
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(editor.remove_button_));
  EXPECT_EQ(2, GetSelectedRowNum(editor));
}

TEST_F(KeywordEditorViewTest, Edit) {
  TemplateURL* a = AddToModel("A", "a", true);
  TemplateURL* b = AddToModel("B", "b", true);
  TemplateURL* c = AddToModel("C", "c", false);
  TemplateURL* d = AddToModel("D", "d", false);
  KeywordEditorView editor(profile_.get());
  EXPECT_STREQ("!,_,A,B (Default),_,@,_,C,D",
               GetDisplayedEngines(editor).c_str());

  editor.OnEditedKeyword(a, ASCIIToUTF16("AA"), ASCIIToUTF16("a"),
                         "example.com/{searchTerms}");
  EXPECT_STREQ("!,_,AA,B (Default),_,@,_,C,D",
               GetDisplayedEngines(editor).c_str());

  editor.OnEditedKeyword(b, ASCIIToUTF16("BB"), ASCIIToUTF16("b"),
                         "foo.example.com/{searchTerms}");
  EXPECT_STREQ("!,_,AA,BB (Default),_,@,_,C,D",
               GetDisplayedEngines(editor).c_str());

  editor.OnEditedKeyword(b, ASCIIToUTF16("BBB"), ASCIIToUTF16("b"),
                         "example.com");
  EXPECT_STREQ("!,_,AA,BBB,_,@,_,C,D",
               GetDisplayedEngines(editor).c_str());

  editor.OnEditedKeyword(d, ASCIIToUTF16("DD"), ASCIIToUTF16("d"),
                         "example.com");
  EXPECT_STREQ("!,_,AA,BBB,_,@,_,C,DD",
               GetDisplayedEngines(editor).c_str());

  editor.OnEditedKeyword(c, ASCIIToUTF16("CC"), ASCIIToUTF16("cc"),
                         "example.com");
  EXPECT_STREQ("!,_,AA,BBB,_,@,_,CC,DD",
               GetDisplayedEngines(editor).c_str());
}
