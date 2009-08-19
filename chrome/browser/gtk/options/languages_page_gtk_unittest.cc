// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/options/languages_page_gtk.h"

#include <gtk/gtk.h>

#include "base/string_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

class LanguagesPageGtkTest : public testing::Test {
 public:
  virtual void SetUp() {
    profile_.reset(new TestingProfile());
  }

  // Get the accept languages displayed in the dialog in the order they are
  // displayed, as a comma seperated string.
  // Ex: EXPECT_STREQ("en,ja", GetDisplayedLangs(page).c_str());
  std::string GetDisplayedLangs(const LanguagesPageGtk& page) {
    std::vector<std::string> parts;
    GtkTreeModel* tree_model = GTK_TREE_MODEL(page.language_order_store_);
    GtkTreeIter iter;
    if (!gtk_tree_model_get_iter_first(tree_model, &iter))
      return std::string();
    while (true) {
      gchar* name;
      gtk_tree_model_get(tree_model, &iter, LanguagesPageGtk::COL_LANG, &name,
                         -1);
      parts.push_back(name);
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

TEST_F(LanguagesPageGtkTest, RemoveAcceptLang) {
  //profile_->GetPrefs()->SetString(prefs::kAcceptLanguages, L"en,ja,es,fr,it");
  profile_->GetPrefs()->SetString(prefs::kAcceptLanguages, L"en,ja,es");
  LanguagesPageGtk page(profile_.get());
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(page.add_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(page.move_up_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(page.move_down_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(page.remove_button_));

  GtkTreeIter iter;
  gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(page.language_order_store_),
                                &iter, NULL, 1);
  gtk_tree_selection_select_iter(page.language_order_selection_, &iter);
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(page.add_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(page.move_up_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(page.move_down_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(page.remove_button_));

  gtk_button_clicked(GTK_BUTTON(page.remove_button_));
  EXPECT_STREQ("English,Spanish", GetDisplayedLangs(page).c_str());
  EXPECT_STREQ("en,es", WideToASCII(
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages)).c_str());
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(page.add_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(page.move_up_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(page.move_down_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(page.remove_button_));

  gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(page.language_order_store_),
                                &iter, NULL, 1);
  gtk_tree_selection_select_iter(page.language_order_selection_, &iter);
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(page.add_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(page.move_up_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(page.move_down_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(page.remove_button_));

  gtk_button_clicked(GTK_BUTTON(page.remove_button_));
  EXPECT_STREQ("English", GetDisplayedLangs(page).c_str());
  EXPECT_STREQ("en", WideToASCII(
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages)).c_str());
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(page.add_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(page.move_up_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(page.move_down_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(page.remove_button_));

  gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(page.language_order_store_),
                                &iter, NULL, 0);
  gtk_tree_selection_select_iter(page.language_order_selection_, &iter);
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(page.add_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(page.move_up_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(page.move_down_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(page.remove_button_));

  gtk_button_clicked(GTK_BUTTON(page.remove_button_));
  EXPECT_STREQ("", GetDisplayedLangs(page).c_str());
  EXPECT_STREQ("", WideToASCII(
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages)).c_str());
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(page.add_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(page.move_up_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(page.move_down_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(page.remove_button_));
}

TEST_F(LanguagesPageGtkTest, RemoveMultipleAcceptLang) {
  profile_->GetPrefs()->SetString(prefs::kAcceptLanguages, L"en,ja,es,fr,it");
  LanguagesPageGtk page(profile_.get());
  GtkTreeIter iter;
  gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(page.language_order_store_),
                                &iter, NULL, 1);
  gtk_tree_selection_select_iter(page.language_order_selection_, &iter);
  gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(page.language_order_store_),
                                &iter, NULL, 3);
  gtk_tree_selection_select_iter(page.language_order_selection_, &iter);
  gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(page.language_order_store_),
                                &iter, NULL, 4);
  gtk_tree_selection_select_iter(page.language_order_selection_, &iter);
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(page.remove_button_));

  gtk_button_clicked(GTK_BUTTON(page.remove_button_));
  EXPECT_STREQ("English,Spanish", GetDisplayedLangs(page).c_str());
  EXPECT_STREQ("en,es", WideToASCII(
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages)).c_str());
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(page.remove_button_));

  gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(page.language_order_store_),
                                &iter, NULL, 1);
  gtk_tree_selection_select_iter(page.language_order_selection_, &iter);
  gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(page.language_order_store_),
                                &iter, NULL, 0);
  gtk_tree_selection_select_iter(page.language_order_selection_, &iter);
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(page.remove_button_));

  gtk_button_clicked(GTK_BUTTON(page.remove_button_));
  EXPECT_STREQ("", GetDisplayedLangs(page).c_str());
  EXPECT_STREQ("", WideToASCII(
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages)).c_str());
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(page.remove_button_));
}

TEST_F(LanguagesPageGtkTest, MoveAcceptLang) {
  profile_->GetPrefs()->SetString(prefs::kAcceptLanguages, L"en,ja,es");
  LanguagesPageGtk page(profile_.get());
  EXPECT_STREQ("English,Japanese,Spanish", GetDisplayedLangs(page).c_str());
  GtkTreeIter iter;

  gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(page.language_order_store_),
                                &iter, NULL, 0);
  gtk_tree_selection_select_iter(page.language_order_selection_, &iter);
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(page.move_up_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(page.move_down_button_));

  gtk_button_clicked(GTK_BUTTON(page.move_down_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(page.move_up_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(page.move_down_button_));
  EXPECT_STREQ("Japanese,English,Spanish", GetDisplayedLangs(page).c_str());
  EXPECT_STREQ("ja,en,es", WideToASCII(
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages)).c_str());

  gtk_button_clicked(GTK_BUTTON(page.move_down_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(page.move_up_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(page.move_down_button_));
  EXPECT_STREQ("Japanese,Spanish,English", GetDisplayedLangs(page).c_str());
  EXPECT_STREQ("ja,es,en", WideToASCII(
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages)).c_str());

  gtk_button_clicked(GTK_BUTTON(page.move_up_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(page.move_up_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(page.move_down_button_));
  EXPECT_STREQ("Japanese,English,Spanish", GetDisplayedLangs(page).c_str());
  EXPECT_STREQ("ja,en,es", WideToASCII(
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages)).c_str());

  gtk_button_clicked(GTK_BUTTON(page.move_up_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(page.move_up_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(page.move_down_button_));
  EXPECT_STREQ("English,Japanese,Spanish", GetDisplayedLangs(page).c_str());
  EXPECT_STREQ("en,ja,es", WideToASCII(
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages)).c_str());
}

TEST_F(LanguagesPageGtkTest, AddAcceptLang) {
  profile_->GetPrefs()->SetString(prefs::kAcceptLanguages, L"");
  LanguagesPageGtk page(profile_.get());
  EXPECT_STREQ("", GetDisplayedLangs(page).c_str());
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(page.add_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(page.move_up_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(page.move_down_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(page.remove_button_));

  page.OnAddLanguage("en");
  EXPECT_STREQ("English", GetDisplayedLangs(page).c_str());
  EXPECT_STREQ("en", WideToASCII(
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages)).c_str());
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(page.add_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(page.move_up_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(page.move_down_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(page.remove_button_));
  EXPECT_EQ(0, page.FirstSelectedRowNum());

  page.OnAddLanguage("es");
  EXPECT_STREQ("English,Spanish", GetDisplayedLangs(page).c_str());
  EXPECT_STREQ("en,es", WideToASCII(
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages)).c_str());
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(page.add_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(page.move_up_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(page.move_down_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(page.remove_button_));
  EXPECT_EQ(1, page.FirstSelectedRowNum());

  // Duplicates should be ignored and selection should not be changed.
  gtk_tree_selection_unselect_all(page.language_order_selection_);
  page.OnAddLanguage("en");
  EXPECT_STREQ("English,Spanish", GetDisplayedLangs(page).c_str());
  EXPECT_STREQ("en,es", WideToASCII(
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages)).c_str());
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(page.add_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(page.move_up_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(page.move_down_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(page.remove_button_));
  EXPECT_EQ(0, gtk_tree_selection_count_selected_rows(
      page.language_order_selection_));
}
