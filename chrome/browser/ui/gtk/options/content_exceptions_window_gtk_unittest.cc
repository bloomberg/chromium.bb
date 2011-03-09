// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/options/content_exceptions_window_gtk.h"

#include "chrome/test/testing_profile.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;

class ContentExceptionsWindowGtkUnittest : public testing::Test {
 public:
  ContentExceptionsWindowGtkUnittest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        host_content_settings_map_(profile_.GetHostContentSettingsMap()),
        window_(NULL) {
  }

  ~ContentExceptionsWindowGtkUnittest() {
    // This will delete window_ too.
    gtk_widget_destroy(window_->dialog_);
    message_loop_.RunAllPending();
  }

  void AddException(const std::string& pattern, ContentSetting value) {
    host_content_settings_map_->SetContentSetting(
        ContentSettingsPattern(pattern),
        CONTENT_SETTINGS_TYPE_JAVASCRIPT,
        "",
        value);
  }

  void BuildWindow() {
    window_ = new ContentExceptionsWindowGtk(
        NULL,
        host_content_settings_map_,
        NULL,
        CONTENT_SETTINGS_TYPE_JAVASCRIPT);
  }

  // Actions:
  void SelectRows(const std::vector<int>& rows) {
    gtk_tree_selection_unselect_all(window_->treeview_selection_);

    for (std::vector<int>::const_iterator it = rows.begin(); it != rows.end();
         ++it) {
      GtkTreeIter iter;
      if (!gtk_tree_model_iter_nth_child(window_->sort_list_store_,
                                         &iter, NULL, *it)) {
        NOTREACHED();
        return;
      }

      gtk_tree_selection_select_iter(window_->treeview_selection_, &iter);
    }
  }

  void Remove() {
    window_->Remove(window_->remove_button_);
  }

  // Getters:

  void GetSelectedRows(std::set<std::pair<int, int> >* selected) {
    window_->GetSelectedModelIndices(selected);
  }

  int StoreListStoreCount() {
    return gtk_tree_model_iter_n_children(
        GTK_TREE_MODEL(window_->sort_list_store_), NULL);
  }

  int ListStoreCount() {
    return gtk_tree_model_iter_n_children(
        GTK_TREE_MODEL(window_->list_store_), NULL);
  }

  std::set<std::string> Paths() {
    std::set<std::string> paths;
    GtkTreeIter iter;
    bool valid = gtk_tree_model_get_iter_first(
        GTK_TREE_MODEL(window_->sort_list_store_), &iter);

    while (valid) {
      gchar* str = NULL;
      gtk_tree_model_get(GTK_TREE_MODEL(window_->sort_list_store_), &iter,
                         0, &str, -1);
      paths.insert(str);
      g_free(str);

      valid = gtk_tree_model_iter_next(
          GTK_TREE_MODEL(window_->sort_list_store_), &iter);
    }

    return paths;
  }

  // Whether certain widgets are enabled:
  bool EditButtonSensitive() {
    return GTK_WIDGET_SENSITIVE(window_->edit_button_);
  }

  bool RemoveButtonSensitive() {
    return GTK_WIDGET_SENSITIVE(window_->remove_button_);
  }

  bool RemoveAllSensitive() {
    return GTK_WIDGET_SENSITIVE(window_->remove_all_button_);
  }

 private:
  MessageLoop message_loop_;
  BrowserThread ui_thread_;

  TestingProfile profile_;
  HostContentSettingsMap* host_content_settings_map_;

  ContentExceptionsWindowGtk* window_;
};

TEST_F(ContentExceptionsWindowGtkUnittest, TestEmptyDisplay) {
  BuildWindow();

  EXPECT_EQ(0, StoreListStoreCount());
  EXPECT_EQ(0, ListStoreCount());

  EXPECT_FALSE(RemoveAllSensitive());
}

TEST_F(ContentExceptionsWindowGtkUnittest, TestDisplay) {
  AddException("a", CONTENT_SETTING_BLOCK);
  AddException("b", CONTENT_SETTING_ALLOW);

  BuildWindow();

  EXPECT_EQ(2, StoreListStoreCount());
  EXPECT_EQ(2, ListStoreCount());

  EXPECT_TRUE(RemoveAllSensitive());
}

TEST_F(ContentExceptionsWindowGtkUnittest, TestSelection) {
  AddException("a", CONTENT_SETTING_BLOCK);

  BuildWindow();

  EXPECT_FALSE(EditButtonSensitive());
  EXPECT_FALSE(RemoveButtonSensitive());

  std::vector<int> rows;
  rows.push_back(0);
  SelectRows(rows);

  EXPECT_TRUE(EditButtonSensitive());
  EXPECT_TRUE(RemoveButtonSensitive());
}

TEST_F(ContentExceptionsWindowGtkUnittest, TestSimpleRemoval) {
  AddException("a", CONTENT_SETTING_BLOCK);
  AddException("b", CONTENT_SETTING_ALLOW);
  AddException("c", CONTENT_SETTING_BLOCK);
  AddException("d", CONTENT_SETTING_ALLOW);

  BuildWindow();

  std::vector<int> rows;
  rows.push_back(1);
  SelectRows(rows);

  Remove();

  EXPECT_EQ(3, StoreListStoreCount());
  EXPECT_EQ(3, ListStoreCount());
  EXPECT_TRUE(EditButtonSensitive());
  EXPECT_TRUE(RemoveButtonSensitive());

  std::set<std::pair<int, int> > selected;
  GetSelectedRows(&selected);

  ASSERT_EQ(1u, selected.size());
  EXPECT_EQ(1, selected.begin()->first);
  EXPECT_EQ(1, selected.begin()->second);
  EXPECT_THAT(Paths(), ElementsAre("a", "c", "d"));
}

TEST_F(ContentExceptionsWindowGtkUnittest, TestComplexRemoval) {
  AddException("a", CONTENT_SETTING_BLOCK);
  AddException("b", CONTENT_SETTING_ALLOW);
  AddException("c", CONTENT_SETTING_BLOCK);
  AddException("d", CONTENT_SETTING_ALLOW);
  AddException("e", CONTENT_SETTING_BLOCK);

  BuildWindow();

  std::vector<int> rows;
  rows.push_back(1);
  rows.push_back(3);
  SelectRows(rows);

  Remove();

  std::set<std::pair<int, int> > selected;
  GetSelectedRows(&selected);

  ASSERT_EQ(1u, selected.size());
  EXPECT_EQ(1, selected.begin()->first);
  EXPECT_EQ(1, selected.begin()->second);
  EXPECT_THAT(Paths(), ElementsAre("a", "c", "e"));
}
