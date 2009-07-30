// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/options/cookies_view.h"

#include <cstdarg>
#include <gtk/gtk.h>

#include "chrome/browser/cookies_table_model.h"
#include "chrome/test/testing_profile.h"
#include "net/url_request/url_request_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestURLRequestContext : public URLRequestContext {
 public:
  TestURLRequestContext() {
    cookie_store_ = new net::CookieMonster();
  }
  virtual ~TestURLRequestContext() {
    delete cookie_store_;
  }
};

class CookieTestingProfile : public TestingProfile {
 public:
  virtual URLRequestContext* GetRequestContext() {
    if (!url_request_context_.get())
      url_request_context_ = new TestURLRequestContext;
    return url_request_context_.get();
  }
  virtual ~CookieTestingProfile() {}

 private:
  scoped_refptr<TestURLRequestContext> url_request_context_;
};

}  // namespace

class CookiesViewTest : public testing::Test {
 public:
  CookiesViewTest() {
  }

  virtual void SetUp() {
    profile_.reset(new CookieTestingProfile());
  }

  virtual void TearDown() {
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

  // Check if the cookie names in the cookie list match the given ones.
  // (Note that the CookieMonster cookie list is sorted by domain.)
  // Ex:
  //   monster->SetCookie(GURL("http://b"), "X=1")
  //   monster->SetCookie(GURL("http://a"), "Y=1")
  //   CheckCookieList(monster, "Y", "X", NULL);
  void CheckCookieList(net::CookieMonster* monster, ...) {
    va_list ap;
    va_start(ap, monster);
    net::CookieMonster::CookieList cookie_list = monster->GetAllCookies();
    size_t i = 0;
    while (const char* text = va_arg(ap, const char*)) {
      ASSERT_LT(i, cookie_list.size());
      EXPECT_EQ(text, cookie_list[i].second.Name());
      ++i;
    }
    va_end(ap);
    EXPECT_EQ(i, cookie_list.size());
  }

  // Check if the cookie names shown in the cookie manager match the given ones.
  // Ex: CheckGtkTree(cookies_view, "X", "Y", NULL);
  void CheckGtkTree(const CookiesView& cookies_view, ...) {
    va_list ap;
    va_start(ap, cookies_view);
    GtkTreeIter iter;
    int i = 0;
    while (const char* text = va_arg(ap, const char*)) {
      ASSERT_TRUE(gtk_tree_model_iter_nth_child(
          cookies_view.list_sort_, &iter, NULL, i));
      gchar* name;
      gtk_tree_model_get(cookies_view.list_sort_, &iter,
                         CookiesView::COL_COOKIE_NAME, &name, -1);
      EXPECT_STREQ(text, name);
      g_free(name);
      ++i;
    }
    va_end(ap);
    EXPECT_EQ(i, gtk_tree_model_iter_n_children(
        GTK_TREE_MODEL(cookies_view.list_store_), NULL));
  }

 protected:
  MessageLoopForUI message_loop_;
  scoped_ptr<CookieTestingProfile> profile_;
};

TEST_F(CookiesViewTest, TestEmpty) {
  CookiesView cookies_view(profile_.get());
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  CheckDetailsSensitivity(FALSE, cookies_view);
  EXPECT_EQ(0, gtk_tree_model_iter_n_children(
      GTK_TREE_MODEL(cookies_view.list_store_), NULL));
}

TEST_F(CookiesViewTest, TestRemoveAll) {
  net::CookieMonster* monster =
      profile_->GetRequestContext()->cookie_store()->GetCookieMonster();
  monster->SetCookie(GURL("http://foo"), "A=1");
  monster->SetCookie(GURL("http://foo2"), "B=1");
  CookiesView cookies_view(profile_.get());

  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  CheckDetailsSensitivity(FALSE, cookies_view);
  EXPECT_EQ(2, gtk_tree_model_iter_n_children(
      GTK_TREE_MODEL(cookies_view.list_store_), NULL));

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_all_button_));
  EXPECT_EQ(0u, monster->GetAllCookies().size());
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  CheckDetailsSensitivity(FALSE, cookies_view);
  EXPECT_EQ(0, gtk_tree_model_iter_n_children(
      GTK_TREE_MODEL(cookies_view.list_store_), NULL));
}

TEST_F(CookiesViewTest, TestRemove) {
  net::CookieMonster* monster =
      profile_->GetRequestContext()->cookie_store()->GetCookieMonster();
  monster->SetCookie(GURL("http://foo1"), "A=1");
  monster->SetCookie(GURL("http://foo2"), "B=1");
  monster->SetCookie(GURL("http://foo3"), "C=1");
  CookiesView cookies_view(profile_.get());
  GtkTreeIter iter;
  gtk_tree_model_iter_nth_child(cookies_view.list_sort_, &iter, NULL, 1);
  gtk_tree_selection_select_iter(cookies_view.selection_, &iter);

  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  CheckDetailsSensitivity(TRUE, cookies_view);
  EXPECT_EQ(3, gtk_tree_model_iter_n_children(
      GTK_TREE_MODEL(cookies_view.list_store_), NULL));

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  CheckCookieList(monster, "A", "C", NULL);
  CheckGtkTree(cookies_view, "A", "C", NULL);
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  CheckDetailsSensitivity(FALSE, cookies_view);

  gtk_tree_model_iter_nth_child(cookies_view.list_sort_, &iter, NULL, 1);
  gtk_tree_selection_select_iter(cookies_view.selection_, &iter);
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  CheckCookieList(monster, "A", NULL);
  CheckGtkTree(cookies_view, "A", NULL);
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  CheckDetailsSensitivity(FALSE, cookies_view);
  EXPECT_EQ(1, gtk_tree_model_iter_n_children(
      GTK_TREE_MODEL(cookies_view.list_store_), NULL));

  gtk_tree_model_iter_nth_child(cookies_view.list_sort_, &iter, NULL, 0);
  gtk_tree_selection_select_iter(cookies_view.selection_, &iter);
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  EXPECT_EQ(0u, monster->GetAllCookies().size());
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  CheckDetailsSensitivity(FALSE, cookies_view);
  EXPECT_EQ(0, gtk_tree_model_iter_n_children(
      GTK_TREE_MODEL(cookies_view.list_store_), NULL));
}

TEST_F(CookiesViewTest, TestFilter) {
  net::CookieMonster* monster =
      profile_->GetRequestContext()->cookie_store()->GetCookieMonster();
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
  CheckGtkTree(cookies_view, "B", "D", NULL);
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));

  gtk_entry_set_text(GTK_ENTRY(cookies_view.filter_entry_), "bar2");
  gtk_widget_activate(cookies_view.filter_entry_);
  CheckGtkTree(cookies_view, "D", NULL);
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));

  gtk_entry_set_text(GTK_ENTRY(cookies_view.filter_entry_), "bar22");
  gtk_widget_activate(cookies_view.filter_entry_);
  EXPECT_EQ(0, gtk_tree_model_iter_n_children(
      GTK_TREE_MODEL(cookies_view.list_store_), NULL));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
}

TEST_F(CookiesViewTest, TestFilterRemoveAll) {
  net::CookieMonster* monster =
      profile_->GetRequestContext()->cookie_store()->GetCookieMonster();
  monster->SetCookie(GURL("http://foo1"), "A=1");
  monster->SetCookie(GURL("http://bar1"), "B=1");
  monster->SetCookie(GURL("http://foo2"), "C=1");
  monster->SetCookie(GURL("http://bar2"), "D=1");
  CookiesView cookies_view(profile_.get());
  gtk_entry_set_text(GTK_ENTRY(cookies_view.filter_entry_), "bar");
  gtk_widget_activate(cookies_view.filter_entry_);
  CheckCookieList(monster, "B", "D", "A", "C", NULL);
  CheckGtkTree(cookies_view, "B", "D", NULL);
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_all_button_));
  CheckCookieList(monster, "A", "C", NULL);
  CheckGtkTree(cookies_view, NULL);
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));

  gtk_entry_set_text(GTK_ENTRY(cookies_view.filter_entry_), "");
  gtk_widget_activate(cookies_view.filter_entry_);
  CheckCookieList(monster, "A", "C", NULL);
  CheckGtkTree(cookies_view, "A", "C", NULL);
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
}

TEST_F(CookiesViewTest, TestFilterRemove) {
  net::CookieMonster* monster =
      profile_->GetRequestContext()->cookie_store()->GetCookieMonster();
  monster->SetCookie(GURL("http://foo1"), "A=1");
  monster->SetCookie(GURL("http://bar1"), "B=1");
  monster->SetCookie(GURL("http://foo2"), "C=1");
  monster->SetCookie(GURL("http://bar2"), "D=1");
  CookiesView cookies_view(profile_.get());
  gtk_entry_set_text(GTK_ENTRY(cookies_view.filter_entry_), "bar");
  gtk_widget_activate(cookies_view.filter_entry_);
  CheckCookieList(monster, "B", "D", "A", "C", NULL);
  CheckGtkTree(cookies_view, "B", "D", NULL);
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));

  GtkTreeIter iter;
  gtk_tree_model_iter_nth_child(cookies_view.list_sort_, &iter, NULL, 0);
  gtk_tree_selection_select_iter(cookies_view.selection_, &iter);
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  CheckCookieList(monster, "D", "A", "C", NULL);
  CheckGtkTree(cookies_view, "D", NULL);
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));

  gtk_tree_model_iter_nth_child(cookies_view.list_sort_, &iter, NULL, 0);
  gtk_tree_selection_select_iter(cookies_view.selection_, &iter);
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  CheckCookieList(monster, "A", "C", NULL);
  CheckGtkTree(cookies_view, NULL);
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
}

TEST_F(CookiesViewTest, TestSort) {
  net::CookieMonster* monster =
      profile_->GetRequestContext()->cookie_store()->GetCookieMonster();
  monster->SetCookie(GURL("http://foo1"), "X=1");
  monster->SetCookie(GURL("http://bar1"), "Z=1");
  monster->SetCookie(GURL("http://foo2"), "C=1");
  monster->SetCookie(GURL("http://bar2"), "D=1");
  CookiesView cookies_view(profile_.get());
  CheckCookieList(monster, "Z", "D", "X", "C", NULL);
  CheckGtkTree(cookies_view, "Z", "D", "X", "C", NULL);

  gtk_tree_sortable_set_sort_column_id(
      GTK_TREE_SORTABLE(cookies_view.list_sort_),
      CookiesView::COL_SITE,
      GTK_SORT_ASCENDING);
  CheckGtkTree(cookies_view, "Z", "D", "X", "C", NULL);

  gtk_tree_sortable_set_sort_column_id(
      GTK_TREE_SORTABLE(cookies_view.list_sort_),
      CookiesView::COL_SITE,
      GTK_SORT_DESCENDING);
  CheckGtkTree(cookies_view, "C", "X", "D", "Z", NULL);

  gtk_tree_sortable_set_sort_column_id(
      GTK_TREE_SORTABLE(cookies_view.list_sort_),
      CookiesView::COL_COOKIE_NAME,
      GTK_SORT_ASCENDING);
  CheckGtkTree(cookies_view, "C", "D", "X", "Z", NULL);

  gtk_tree_sortable_set_sort_column_id(
      GTK_TREE_SORTABLE(cookies_view.list_sort_),
      CookiesView::COL_COOKIE_NAME,
      GTK_SORT_DESCENDING);
  CheckGtkTree(cookies_view, "Z", "X", "D", "C", NULL);
}

TEST_F(CookiesViewTest, TestSortRemove) {
  net::CookieMonster* monster =
      profile_->GetRequestContext()->cookie_store()->GetCookieMonster();
  monster->SetCookie(GURL("http://foo1"), "B=1");
  monster->SetCookie(GURL("http://bar1"), "Z=1");
  monster->SetCookie(GURL("http://foo2"), "C=1");
  monster->SetCookie(GURL("http://bar2"), "A=1");
  CookiesView cookies_view(profile_.get());
  CheckCookieList(monster, "Z", "A", "B", "C", NULL);
  CheckGtkTree(cookies_view, "Z", "A", "B", "C", NULL);

  gtk_tree_sortable_set_sort_column_id(
      GTK_TREE_SORTABLE(cookies_view.list_sort_),
      CookiesView::COL_COOKIE_NAME,
      GTK_SORT_DESCENDING);
  CheckGtkTree(cookies_view, "Z", "C", "B", "A", NULL);

  GtkTreeIter iter;
  gtk_tree_model_iter_nth_child(cookies_view.list_sort_, &iter, NULL, 3);
  gtk_tree_selection_select_iter(cookies_view.selection_, &iter);
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));
  CheckCookieList(monster, "Z", "B", "C", NULL);
  CheckGtkTree(cookies_view, "Z", "C", "B", NULL);
}

TEST_F(CookiesViewTest, TestSortFilterRemove) {
  net::CookieMonster* monster =
      profile_->GetRequestContext()->cookie_store()->GetCookieMonster();
  monster->SetCookie(GURL("http://foo1"), "B=1");
  monster->SetCookie(GURL("http://bar1"), "Z=1");
  monster->SetCookie(GURL("http://foo2"), "C=1");
  monster->SetCookie(GURL("http://bar2"), "A=1");
  CookiesView cookies_view(profile_.get());
  CheckCookieList(monster, "Z", "A", "B", "C", NULL);
  CheckGtkTree(cookies_view, "Z", "A", "B", "C", NULL);

  gtk_entry_set_text(GTK_ENTRY(cookies_view.filter_entry_), "bar");
  gtk_widget_activate(cookies_view.filter_entry_);
  gtk_tree_sortable_set_sort_column_id(
      GTK_TREE_SORTABLE(cookies_view.list_sort_),
      CookiesView::COL_COOKIE_NAME,
      GTK_SORT_ASCENDING);
  CheckGtkTree(cookies_view, "A", "Z", NULL);

  GtkTreeIter iter;
  gtk_tree_model_iter_nth_child(cookies_view.list_sort_, &iter, NULL, 1);
  gtk_tree_selection_select_iter(cookies_view.selection_, &iter);
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));
  CheckCookieList(monster, "A", "B", "C", NULL);
  CheckGtkTree(cookies_view, "A", NULL);

  gtk_tree_model_iter_nth_child(cookies_view.list_sort_, &iter, NULL, 0);
  gtk_tree_selection_select_iter(cookies_view.selection_, &iter);
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));
  CheckCookieList(monster, "B", "C", NULL);
  CheckGtkTree(cookies_view, NULL);
}
