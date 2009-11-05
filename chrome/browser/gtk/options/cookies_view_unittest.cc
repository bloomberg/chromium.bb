// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/options/cookies_view.h"

#include <string>
#include <vector>

#include <gtk/gtk.h>

#include "base/string_util.h"
#include "chrome/browser/cookies_table_model.h"
#include "chrome/browser/net/url_request_context_getter.h"
#include "chrome/test/testing_profile.h"
#include "net/url_request/url_request_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestURLRequestContext : public URLRequestContext {
 public:
  TestURLRequestContext() {
    cookie_store_ = new net::CookieMonster();
  }

 private:
  ~TestURLRequestContext() {}
};

class TestURLRequestContextGetter : public URLRequestContextGetter {
 public:
  virtual URLRequestContext* GetURLRequestContext() {
    if (!context_)
      context_ = new TestURLRequestContext();
    return context_;
  }
 private:
  scoped_refptr<URLRequestContext> context_;
};

class CookieTestingProfile : public TestingProfile {
 public:
  virtual URLRequestContextGetter* GetRequestContext() {
    if (!url_request_context_getter_.get())
      url_request_context_getter_ = new TestURLRequestContextGetter;
    return url_request_context_getter_.get();
  }
  virtual ~CookieTestingProfile() {}

  net::CookieMonster* GetCookieMonster() {
    return GetRequestContext()->GetCookieStore()->GetCookieMonster();
  }

 private:
  scoped_refptr<URLRequestContextGetter> url_request_context_getter_;
};

}  // namespace

class CookiesViewTest : public testing::Test {
 public:
  virtual void SetUp() {
    profile_.reset(new CookieTestingProfile());
  }

  void CheckDetailsSensitivity(gboolean expected,
                               const CookiesView& cookies_view) {
    EXPECT_EQ(expected,
              GTK_WIDGET_SENSITIVE(cookies_view.cookie_name_entry_));
    EXPECT_EQ(expected,
              GTK_WIDGET_SENSITIVE(cookies_view.cookie_content_entry_));
    EXPECT_EQ(expected,
              GTK_WIDGET_SENSITIVE(cookies_view.cookie_domain_entry_));
    EXPECT_EQ(expected,
              GTK_WIDGET_SENSITIVE(cookies_view.cookie_path_entry_));
    EXPECT_EQ(expected,
              GTK_WIDGET_SENSITIVE(cookies_view.cookie_send_for_entry_));
    EXPECT_EQ(expected,
              GTK_WIDGET_SENSITIVE(cookies_view.cookie_created_entry_));
    EXPECT_EQ(expected,
              GTK_WIDGET_SENSITIVE(cookies_view.cookie_expires_entry_));
  }

  // Get the cookie names in the cookie list, as a comma seperated string.
  // (Note that the CookieMonster cookie list is sorted by domain.)
  // Ex:
  //   monster->SetCookie(GURL("http://b"), "X=1")
  //   monster->SetCookie(GURL("http://a"), "Y=1")
  //   EXPECT_STREQ("Y,X", GetMonsterCookies(monster).c_str());
  std::string GetMonsterCookies(net::CookieMonster* monster) {
    std::vector<std::string> parts;
    net::CookieMonster::CookieList cookie_list = monster->GetAllCookies();
    for (size_t i = 0; i < cookie_list.size(); ++i)
      parts.push_back(cookie_list[i].second.Name());
    return JoinString(parts, ',');
  }

  // Get the cookie names displayed in the dialog in the order they are
  // displayed, as a comma seperated string.
  // Ex: EXPECT_STREQ("X,Y", GetDisplayedCookies(cookies_view).c_str());
  std::string GetDisplayedCookies(const CookiesView& cookies_view) {
    std::vector<std::string> parts;
    GtkTreeIter iter;
    if (!gtk_tree_model_get_iter_first(cookies_view.list_sort_, &iter))
      return std::string();
    while (true) {
      gchar* name;
      gtk_tree_model_get(cookies_view.list_sort_, &iter,
                         CookiesView::COL_COOKIE_NAME, &name, -1);
      parts.push_back(name);
      g_free(name);
      if (!gtk_tree_model_iter_next(cookies_view.list_sort_, &iter))
        break;
    }
    return JoinString(parts, ',');
  }

 protected:
  MessageLoopForUI message_loop_;
  scoped_ptr<CookieTestingProfile> profile_;
};

TEST_F(CookiesViewTest, Empty) {
  CookiesView cookies_view(profile_.get());
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  CheckDetailsSensitivity(FALSE, cookies_view);
  EXPECT_EQ(0, gtk_tree_model_iter_n_children(
      GTK_TREE_MODEL(cookies_view.list_store_), NULL));
}

TEST_F(CookiesViewTest, RemoveAll) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo"), "A=1");
  monster->SetCookie(GURL("http://foo2"), "B=1");
  CookiesView cookies_view(profile_.get());

  // Reset the selection of the first row.
  gtk_tree_selection_unselect_all(cookies_view.selection_);

  {
    SCOPED_TRACE("Before removing");
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
    EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
    CheckDetailsSensitivity(FALSE, cookies_view);
    EXPECT_EQ(2, gtk_tree_model_iter_n_children(
        GTK_TREE_MODEL(cookies_view.list_store_), NULL));
  }

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_all_button_));
  {
    SCOPED_TRACE("After removing");
    EXPECT_EQ(0u, monster->GetAllCookies().size());
    EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
    EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
    CheckDetailsSensitivity(FALSE, cookies_view);
    EXPECT_EQ(0, gtk_tree_model_iter_n_children(
        GTK_TREE_MODEL(cookies_view.list_store_), NULL));
  }
}

// When removing all items, if multiple items were selected the
// OnSelectionChanged callback could get called while the gtk list view and the
// CookiesTableModel were inconsistent.  Test that it doesn't crash.
TEST_F(CookiesViewTest, RemoveAllWithAllSelected) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo"), "A=1");
  monster->SetCookie(GURL("http://foo2"), "B=1");
  CookiesView cookies_view(profile_.get());

  gtk_tree_selection_select_all(cookies_view.selection_);
  {
    SCOPED_TRACE("Before removing");
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
    CheckDetailsSensitivity(FALSE, cookies_view);
    EXPECT_EQ(2, gtk_tree_model_iter_n_children(
        GTK_TREE_MODEL(cookies_view.list_store_), NULL));
  }

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_all_button_));
  {
    SCOPED_TRACE("After removing");
    EXPECT_EQ(0u, monster->GetAllCookies().size());
    EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
    EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
    CheckDetailsSensitivity(FALSE, cookies_view);
    EXPECT_EQ(0, gtk_tree_model_iter_n_children(
        GTK_TREE_MODEL(cookies_view.list_store_), NULL));
  }
}

TEST_F(CookiesViewTest, Remove) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo1"), "A=1");
  monster->SetCookie(GURL("http://foo2"), "B=1");
  monster->SetCookie(GURL("http://foo3"), "C=1");
  CookiesView cookies_view(profile_.get());

  // Reset the selection of the first row.
  gtk_tree_selection_unselect_all(cookies_view.selection_);

  GtkTreeIter iter;
  gtk_tree_model_iter_nth_child(cookies_view.list_sort_, &iter, NULL, 1);
  gtk_tree_selection_select_iter(cookies_view.selection_, &iter);

  {
    SCOPED_TRACE("First selection");
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
    CheckDetailsSensitivity(TRUE, cookies_view);
    EXPECT_EQ(3, gtk_tree_model_iter_n_children(
        GTK_TREE_MODEL(cookies_view.list_store_), NULL));
  }

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  {
    SCOPED_TRACE("First selection removed");
    EXPECT_STREQ("A,C", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("A,C", GetDisplayedCookies(cookies_view).c_str());
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
    EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
    CheckDetailsSensitivity(FALSE, cookies_view);
  }

  gtk_tree_model_iter_nth_child(cookies_view.list_sort_, &iter, NULL, 1);
  gtk_tree_selection_select_iter(cookies_view.selection_, &iter);
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  {
    SCOPED_TRACE("Second selection");
    EXPECT_STREQ("A", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("A", GetDisplayedCookies(cookies_view).c_str());
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
    EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
    CheckDetailsSensitivity(FALSE, cookies_view);
    EXPECT_EQ(1, gtk_tree_model_iter_n_children(
        GTK_TREE_MODEL(cookies_view.list_store_), NULL));
  }

  gtk_tree_model_iter_nth_child(cookies_view.list_sort_, &iter, NULL, 0);
  gtk_tree_selection_select_iter(cookies_view.selection_, &iter);
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  {
    SCOPED_TRACE("Second selection removed");
    EXPECT_EQ(0u, monster->GetAllCookies().size());
    EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
    EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
    CheckDetailsSensitivity(FALSE, cookies_view);
    EXPECT_EQ(0, gtk_tree_model_iter_n_children(
        GTK_TREE_MODEL(cookies_view.list_store_), NULL));
  }
}

TEST_F(CookiesViewTest, RemoveMultiple) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo0"), "C=1");
  monster->SetCookie(GURL("http://foo1"), "D=1");
  monster->SetCookie(GURL("http://foo2"), "B=1");
  monster->SetCookie(GURL("http://foo3"), "A=1");
  monster->SetCookie(GURL("http://foo4"), "E=1");
  monster->SetCookie(GURL("http://foo5"), "G=1");
  monster->SetCookie(GURL("http://foo6"), "X=1");
  CookiesView cookies_view(profile_.get());

  // Reset the selection of the first row.
  gtk_tree_selection_unselect_all(cookies_view.selection_);

  GtkTreeIter iter;
  gtk_tree_model_iter_nth_child(cookies_view.list_sort_, &iter, NULL, 1);
  gtk_tree_selection_select_iter(cookies_view.selection_, &iter);
  gtk_tree_model_iter_nth_child(cookies_view.list_sort_, &iter, NULL, 3);
  gtk_tree_selection_select_iter(cookies_view.selection_, &iter);
  gtk_tree_model_iter_nth_child(cookies_view.list_sort_, &iter, NULL, 4);
  gtk_tree_selection_select_iter(cookies_view.selection_, &iter);
  gtk_tree_model_iter_nth_child(cookies_view.list_sort_, &iter, NULL, 5);
  gtk_tree_selection_select_iter(cookies_view.selection_, &iter);

  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  EXPECT_STREQ("C,B,X", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("C,B,X", GetDisplayedCookies(cookies_view).c_str());
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  EXPECT_EQ(0, gtk_tree_selection_count_selected_rows(cookies_view.selection_));
}

TEST_F(CookiesViewTest, RemoveDefaultSelection) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo1"), "A=1");
  monster->SetCookie(GURL("http://foo2"), "B=1");
  monster->SetCookie(GURL("http://foo3"), "C=1");
  // Now CookiesView select the first row when it is opened.
  CookiesView cookies_view(profile_.get());

  {
    SCOPED_TRACE("First selection");
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
    CheckDetailsSensitivity(TRUE, cookies_view);
    EXPECT_EQ(3, gtk_tree_model_iter_n_children(
        GTK_TREE_MODEL(cookies_view.list_store_), NULL));
  }

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  {
    SCOPED_TRACE("First selection removed");
    EXPECT_STREQ("B,C", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("B,C", GetDisplayedCookies(cookies_view).c_str());
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
    EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
    CheckDetailsSensitivity(FALSE, cookies_view);
  }
}

TEST_F(CookiesViewTest, Filter) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo1"), "A=1");
  monster->SetCookie(GURL("http://bar1"), "B=1");
  monster->SetCookie(GURL("http://foo2"), "C=1");
  monster->SetCookie(GURL("http://bar2"), "D=1");
  CookiesView cookies_view(profile_.get());
  EXPECT_EQ(4, gtk_tree_model_iter_n_children(
      GTK_TREE_MODEL(cookies_view.list_store_), NULL));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));

  gtk_entry_set_text(GTK_ENTRY(cookies_view.filter_entry_), "bar");
  // Entering text doesn't immediately filter the results.
  EXPECT_EQ(4, gtk_tree_model_iter_n_children(
      GTK_TREE_MODEL(cookies_view.list_store_), NULL));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));

  // Results are filtered immediately if you activate (hit enter in the entry.)
  gtk_widget_activate(cookies_view.filter_entry_);
  EXPECT_STREQ("B,D", GetDisplayedCookies(cookies_view).c_str());
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));

  gtk_entry_set_text(GTK_ENTRY(cookies_view.filter_entry_), "bar2");
  gtk_widget_activate(cookies_view.filter_entry_);
  EXPECT_STREQ("D", GetDisplayedCookies(cookies_view).c_str());
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));

  gtk_entry_set_text(GTK_ENTRY(cookies_view.filter_entry_), "bar22");
  gtk_widget_activate(cookies_view.filter_entry_);
  EXPECT_EQ(0, gtk_tree_model_iter_n_children(
      GTK_TREE_MODEL(cookies_view.list_store_), NULL));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
}

TEST_F(CookiesViewTest, FilterRemoveAll) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo1"), "A=1");
  monster->SetCookie(GURL("http://bar1"), "B=1");
  monster->SetCookie(GURL("http://foo2"), "C=1");
  monster->SetCookie(GURL("http://bar2"), "D=1");
  CookiesView cookies_view(profile_.get());
  gtk_entry_set_text(GTK_ENTRY(cookies_view.filter_entry_), "bar");
  gtk_widget_activate(cookies_view.filter_entry_);
  EXPECT_STREQ("B,D,A,C", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("B,D", GetDisplayedCookies(cookies_view).c_str());
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_all_button_));
  EXPECT_STREQ("A,C", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("", GetDisplayedCookies(cookies_view).c_str());
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));

  gtk_entry_set_text(GTK_ENTRY(cookies_view.filter_entry_), "");
  gtk_widget_activate(cookies_view.filter_entry_);
  EXPECT_STREQ("A,C", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("A,C", GetDisplayedCookies(cookies_view).c_str());
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
}

TEST_F(CookiesViewTest, FilterRemove) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo1"), "A=1");
  monster->SetCookie(GURL("http://bar1"), "B=1");
  monster->SetCookie(GURL("http://foo2"), "C=1");
  monster->SetCookie(GURL("http://bar2"), "D=1");
  CookiesView cookies_view(profile_.get());
  gtk_entry_set_text(GTK_ENTRY(cookies_view.filter_entry_), "bar");
  gtk_widget_activate(cookies_view.filter_entry_);
  EXPECT_STREQ("B,D,A,C", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("B,D", GetDisplayedCookies(cookies_view).c_str());
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));

  GtkTreeIter iter;
  gtk_tree_model_iter_nth_child(cookies_view.list_sort_, &iter, NULL, 0);
  gtk_tree_selection_select_iter(cookies_view.selection_, &iter);
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  EXPECT_STREQ("D,A,C", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("D", GetDisplayedCookies(cookies_view).c_str());
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));

  gtk_tree_model_iter_nth_child(cookies_view.list_sort_, &iter, NULL, 0);
  gtk_tree_selection_select_iter(cookies_view.selection_, &iter);
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  EXPECT_STREQ("A,C", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("", GetDisplayedCookies(cookies_view).c_str());
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
}

TEST_F(CookiesViewTest, Sort) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo1"), "X=1");
  monster->SetCookie(GURL("http://bar1"), "Z=1");
  monster->SetCookie(GURL("http://foo2"), "C=1");
  monster->SetCookie(GURL("http://bar2"), "D=1");
  CookiesView cookies_view(profile_.get());
  EXPECT_STREQ("Z,D,X,C", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("Z,D,X,C", GetDisplayedCookies(cookies_view).c_str());

  gtk_tree_sortable_set_sort_column_id(
      GTK_TREE_SORTABLE(cookies_view.list_sort_),
      CookiesView::COL_SITE,
      GTK_SORT_ASCENDING);
  EXPECT_STREQ("Z,D,X,C", GetDisplayedCookies(cookies_view).c_str());

  gtk_tree_sortable_set_sort_column_id(
      GTK_TREE_SORTABLE(cookies_view.list_sort_),
      CookiesView::COL_SITE,
      GTK_SORT_DESCENDING);
  EXPECT_STREQ("C,X,D,Z", GetDisplayedCookies(cookies_view).c_str());

  gtk_tree_sortable_set_sort_column_id(
      GTK_TREE_SORTABLE(cookies_view.list_sort_),
      CookiesView::COL_COOKIE_NAME,
      GTK_SORT_ASCENDING);
  EXPECT_STREQ("C,D,X,Z", GetDisplayedCookies(cookies_view).c_str());

  gtk_tree_sortable_set_sort_column_id(
      GTK_TREE_SORTABLE(cookies_view.list_sort_),
      CookiesView::COL_COOKIE_NAME,
      GTK_SORT_DESCENDING);
  EXPECT_STREQ("Z,X,D,C", GetDisplayedCookies(cookies_view).c_str());
}

TEST_F(CookiesViewTest, SortRemove) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo1"), "B=1");
  monster->SetCookie(GURL("http://bar1"), "Z=1");
  monster->SetCookie(GURL("http://foo2"), "C=1");
  monster->SetCookie(GURL("http://bar2"), "A=1");
  CookiesView cookies_view(profile_.get());
  EXPECT_STREQ("Z,A,B,C", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("Z,A,B,C", GetDisplayedCookies(cookies_view).c_str());

  gtk_tree_sortable_set_sort_column_id(
      GTK_TREE_SORTABLE(cookies_view.list_sort_),
      CookiesView::COL_COOKIE_NAME,
      GTK_SORT_DESCENDING);
  EXPECT_STREQ("Z,C,B,A", GetDisplayedCookies(cookies_view).c_str());

  // Reset the selection of the first row.
  gtk_tree_selection_unselect_all(cookies_view.selection_);

  GtkTreeIter iter;
  gtk_tree_model_iter_nth_child(cookies_view.list_sort_, &iter, NULL, 3);
  gtk_tree_selection_select_iter(cookies_view.selection_, &iter);
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));
  EXPECT_STREQ("Z,B,C", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("Z,C,B", GetDisplayedCookies(cookies_view).c_str());
}

TEST_F(CookiesViewTest, SortFilterRemove) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo1"), "B=1");
  monster->SetCookie(GURL("http://bar1"), "Z=1");
  monster->SetCookie(GURL("http://foo2"), "C=1");
  monster->SetCookie(GURL("http://bar2"), "A=1");
  CookiesView cookies_view(profile_.get());
  EXPECT_STREQ("Z,A,B,C", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("Z,A,B,C", GetDisplayedCookies(cookies_view).c_str());

  gtk_entry_set_text(GTK_ENTRY(cookies_view.filter_entry_), "bar");
  gtk_widget_activate(cookies_view.filter_entry_);
  gtk_tree_sortable_set_sort_column_id(
      GTK_TREE_SORTABLE(cookies_view.list_sort_),
      CookiesView::COL_COOKIE_NAME,
      GTK_SORT_ASCENDING);
  EXPECT_STREQ("A,Z", GetDisplayedCookies(cookies_view).c_str());

  GtkTreeIter iter;
  gtk_tree_model_iter_nth_child(cookies_view.list_sort_, &iter, NULL, 1);
  gtk_tree_selection_select_iter(cookies_view.selection_, &iter);
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));
  EXPECT_STREQ("A,B,C", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("A", GetDisplayedCookies(cookies_view).c_str());

  gtk_tree_model_iter_nth_child(cookies_view.list_sort_, &iter, NULL, 0);
  gtk_tree_selection_select_iter(cookies_view.selection_, &iter);
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));
  EXPECT_STREQ("B,C", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("", GetDisplayedCookies(cookies_view).c_str());
}

TEST_F(CookiesViewTest, SortRemoveMultiple) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo0"), "C=1");
  monster->SetCookie(GURL("http://foo1"), "D=1");
  monster->SetCookie(GURL("http://foo2"), "B=1");
  monster->SetCookie(GURL("http://foo3"), "A=1");
  monster->SetCookie(GURL("http://foo4"), "E=1");
  monster->SetCookie(GURL("http://foo5"), "G=1");
  monster->SetCookie(GURL("http://foo6"), "X=1");
  CookiesView cookies_view(profile_.get());
  gtk_tree_sortable_set_sort_column_id(
      GTK_TREE_SORTABLE(cookies_view.list_sort_),
      CookiesView::COL_COOKIE_NAME,
      GTK_SORT_DESCENDING);
  EXPECT_STREQ("X,G,E,D,C,B,A", GetDisplayedCookies(cookies_view).c_str());
  EXPECT_STREQ("C,D,B,A,E,G,X", GetMonsterCookies(monster).c_str());

  GtkTreeIter iter;
  gtk_tree_model_iter_nth_child(cookies_view.list_sort_, &iter, NULL, 1);
  gtk_tree_selection_select_iter(cookies_view.selection_, &iter);
  gtk_tree_model_iter_nth_child(cookies_view.list_sort_, &iter, NULL, 3);
  gtk_tree_selection_select_iter(cookies_view.selection_, &iter);
  gtk_tree_model_iter_nth_child(cookies_view.list_sort_, &iter, NULL, 4);
  gtk_tree_selection_select_iter(cookies_view.selection_, &iter);
  gtk_tree_model_iter_nth_child(cookies_view.list_sort_, &iter, NULL, 5);
  gtk_tree_selection_select_iter(cookies_view.selection_, &iter);

  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  EXPECT_STREQ("X,E,A", GetDisplayedCookies(cookies_view).c_str());
  EXPECT_STREQ("A,E,X", GetMonsterCookies(monster).c_str());
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  EXPECT_EQ(0, gtk_tree_selection_count_selected_rows(cookies_view.selection_));
}

TEST_F(CookiesViewTest, SortRemoveDefaultSelection) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo1"), "Z=1");
  monster->SetCookie(GURL("http://bar1"), "X=1");
  monster->SetCookie(GURL("http://foo2"), "W=1");
  monster->SetCookie(GURL("http://bar2"), "Y=1");
  CookiesView cookies_view(profile_.get());
  EXPECT_STREQ("X,Y,Z,W", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("X,Y,Z,W", GetDisplayedCookies(cookies_view).c_str());

  gtk_tree_sortable_set_sort_column_id(
      GTK_TREE_SORTABLE(cookies_view.list_sort_),
      CookiesView::COL_COOKIE_NAME,
      GTK_SORT_ASCENDING);
  EXPECT_STREQ("W,X,Y,Z", GetDisplayedCookies(cookies_view).c_str());

  GtkTreeIter iter;
  gtk_tree_model_iter_nth_child(cookies_view.list_sort_, &iter, NULL, 3);
  gtk_tree_selection_select_iter(cookies_view.selection_, &iter);

  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));
  EXPECT_STREQ("Y,W", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("W,Y", GetDisplayedCookies(cookies_view).c_str());
}
