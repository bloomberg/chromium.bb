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
  ~TestURLRequestContextGetter() {}

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

  static gboolean GetDisplayedCookiesHelper(GtkTreeModel *model,
                                            GtkTreePath *path,
                                            GtkTreeIter *iter,
                                            gpointer user_data) {
    gchar* title;
    gtk_tree_model_get(model, iter,
                       gtk_tree::TreeAdapter::COL_TITLE, &title,
                       -1);
    std::string str;
    str.append(gtk_tree_path_get_depth(path) - 1, '_');
    str += title;
    g_free(title);
    std::vector<std::string>* parts =
        reinterpret_cast<std::vector<std::string>*>(user_data);
    parts->push_back(str);
    return FALSE;
  }

  // Get the cookie names displayed in the dialog in the order they are
  // displayed, as a comma seperated string.
  // Ex: EXPECT_STREQ("X,Y", GetDisplayedCookies(cookies_view).c_str());
  std::string GetDisplayedCookies(const CookiesView& cookies_view) {
    GtkTreeStore* tree_store = cookies_view.cookies_tree_adapter_->tree_store();
    std::vector<std::string> parts;
    gtk_tree_model_foreach(GTK_TREE_MODEL(tree_store),
                           GetDisplayedCookiesHelper, &parts);
    return JoinString(parts, ',');
  }

  bool SelectByPath(const CookiesView& cookies_view, const char* path_str) {
    GtkTreePath* path = gtk_tree_path_new_from_string(path_str);
    if (!path)
      return false;
    gtk_tree_selection_select_path(cookies_view.selection_, path);
    gtk_tree_path_free(path);
    return true;
  }

  std::string GetSelectedPath(const CookiesView& cookies_view) {
    std::string result;
    GtkTreeIter iter;
    bool selected = gtk_tree_selection_get_selected(cookies_view.selection_,
                                                    NULL, &iter);
    if (selected) {
      gchar* path_str = gtk_tree_model_get_string_from_iter(
          GTK_TREE_MODEL(cookies_view.cookies_tree_adapter_->tree_store()),
          &iter);
      if (path_str) {
        result = path_str;
        g_free(path_str);
      }
    }
    return result;
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
  EXPECT_STREQ("", GetDisplayedCookies(cookies_view).c_str());
}

TEST_F(CookiesViewTest, Noop) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo0"), "C=1");
  monster->SetCookie(GURL("http://foo0"), "D=1");
  monster->SetCookie(GURL("http://foo1"), "B=1");
  monster->SetCookie(GURL("http://foo1"), "A=1");
  monster->SetCookie(GURL("http://foo1"), "E=1");
  monster->SetCookie(GURL("http://foo2"), "G=1");
  monster->SetCookie(GURL("http://foo2"), "X=1");
  CookiesView cookies_view(profile_.get());
  EXPECT_STREQ("foo0,_Cookies,__C,__D,"
               "foo1,_Cookies,__A,__B,__E,"
               "foo2,_Cookies,__G,__X",
               GetDisplayedCookies(cookies_view).c_str());
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  CheckDetailsSensitivity(FALSE, cookies_view);
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
    EXPECT_STREQ("foo,_Cookies,__A,foo2,_Cookies,__B",
                 GetDisplayedCookies(cookies_view).c_str());
  }

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_all_button_));
  {
    SCOPED_TRACE("After removing");
    EXPECT_EQ(0u, monster->GetAllCookies().size());
    EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
    EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
    CheckDetailsSensitivity(FALSE, cookies_view);
    EXPECT_STREQ("", GetDisplayedCookies(cookies_view).c_str());
  }
}

TEST_F(CookiesViewTest, RemoveAllWithDefaultSelected) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo"), "A=1");
  monster->SetCookie(GURL("http://foo2"), "B=1");
  CookiesView cookies_view(profile_.get());

  EXPECT_STREQ("0", GetSelectedPath(cookies_view).c_str());
  EXPECT_EQ(1, gtk_tree_selection_count_selected_rows(cookies_view.selection_));
  {
    SCOPED_TRACE("Before removing");
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
    CheckDetailsSensitivity(FALSE, cookies_view);
    EXPECT_STREQ("foo,_Cookies,__A,foo2,_Cookies,__B",
                 GetDisplayedCookies(cookies_view).c_str());
  }

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_all_button_));
  {
    SCOPED_TRACE("After removing");
    EXPECT_EQ(0u, monster->GetAllCookies().size());
    EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
    EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
    CheckDetailsSensitivity(FALSE, cookies_view);
    EXPECT_STREQ("", GetDisplayedCookies(cookies_view).c_str());
    EXPECT_EQ(0,
              gtk_tree_selection_count_selected_rows(cookies_view.selection_));
  }
}

TEST_F(CookiesViewTest, Remove) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo1"), "A=1");
  monster->SetCookie(GURL("http://foo2"), "B=1");
  monster->SetCookie(GURL("http://foo2"), "C=1");
  CookiesView cookies_view(profile_.get());

  ASSERT_TRUE(SelectByPath(cookies_view, "1:0:0"));

  {
    SCOPED_TRACE("First selection");
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
    CheckDetailsSensitivity(TRUE, cookies_view);
    EXPECT_STREQ("foo1,_Cookies,__A,foo2,_Cookies,__B,__C",
                 GetDisplayedCookies(cookies_view).c_str());
  }

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  {
    SCOPED_TRACE("First selection removed");
    EXPECT_STREQ("A,C", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("foo1,_Cookies,__A,foo2,_Cookies,__C",
                 GetDisplayedCookies(cookies_view).c_str());
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
    EXPECT_STREQ("1:0:0", GetSelectedPath(cookies_view).c_str());
    CheckDetailsSensitivity(TRUE, cookies_view);
  }

  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  {
    SCOPED_TRACE("Second selection");
    EXPECT_STREQ("A", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("foo1,_Cookies,__A,foo2,_Cookies",
                 GetDisplayedCookies(cookies_view).c_str());
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
    EXPECT_STREQ("1:0", GetSelectedPath(cookies_view).c_str());
    CheckDetailsSensitivity(FALSE, cookies_view);
  }

  ASSERT_TRUE(SelectByPath(cookies_view, "0:0:0"));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  {
    SCOPED_TRACE("Second selection removed");
    EXPECT_EQ(0u, monster->GetAllCookies().size());
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
    EXPECT_STREQ("0:0", GetSelectedPath(cookies_view).c_str());
    CheckDetailsSensitivity(FALSE, cookies_view);
    EXPECT_STREQ("foo1,_Cookies,foo2,_Cookies",
                 GetDisplayedCookies(cookies_view).c_str());
  }
}

TEST_F(CookiesViewTest, RemoveCookiesByDomain) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo0"), "C=1");
  monster->SetCookie(GURL("http://foo0"), "D=1");
  monster->SetCookie(GURL("http://foo1"), "B=1");
  monster->SetCookie(GURL("http://foo1"), "A=1");
  monster->SetCookie(GURL("http://foo1"), "E=1");
  monster->SetCookie(GURL("http://foo2"), "G=1");
  monster->SetCookie(GURL("http://foo2"), "X=1");
  CookiesView cookies_view(profile_.get());
  EXPECT_STREQ("foo0,_Cookies,__C,__D,"
               "foo1,_Cookies,__A,__B,__E,"
               "foo2,_Cookies,__G,__X",
               GetDisplayedCookies(cookies_view).c_str());

  ASSERT_TRUE(SelectByPath(cookies_view, "1:0"));

  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  EXPECT_STREQ("C,D,G,X", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("foo0,_Cookies,__C,__D,"
               "foo1,"
               "foo2,_Cookies,__G,__X",
               GetDisplayedCookies(cookies_view).c_str());
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  EXPECT_STREQ("1", GetSelectedPath(cookies_view).c_str());

  ASSERT_TRUE(SelectByPath(cookies_view, "0:0"));
  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  EXPECT_STREQ("G,X", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("foo0,"
               "foo1,"
               "foo2,_Cookies,__G,__X",
               GetDisplayedCookies(cookies_view).c_str());
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  EXPECT_STREQ("0", GetSelectedPath(cookies_view).c_str());

  ASSERT_TRUE(SelectByPath(cookies_view, "2:0"));
  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  EXPECT_STREQ("", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("foo0,"
               "foo1,"
               "foo2",
               GetDisplayedCookies(cookies_view).c_str());
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  EXPECT_STREQ("2", GetSelectedPath(cookies_view).c_str());
}

TEST_F(CookiesViewTest, RemoveByDomain) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo0"), "C=1");
  monster->SetCookie(GURL("http://foo0"), "D=1");
  monster->SetCookie(GURL("http://foo1"), "B=1");
  monster->SetCookie(GURL("http://foo1"), "A=1");
  monster->SetCookie(GURL("http://foo1"), "E=1");
  monster->SetCookie(GURL("http://foo2"), "G=1");
  monster->SetCookie(GURL("http://foo2"), "X=1");
  CookiesView cookies_view(profile_.get());
  EXPECT_STREQ("foo0,_Cookies,__C,__D,"
               "foo1,_Cookies,__A,__B,__E,"
               "foo2,_Cookies,__G,__X",
               GetDisplayedCookies(cookies_view).c_str());

  ASSERT_TRUE(SelectByPath(cookies_view, "1"));

  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  EXPECT_STREQ("C,D,G,X", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("foo0,_Cookies,__C,__D,"
               "foo2,_Cookies,__G,__X",
               GetDisplayedCookies(cookies_view).c_str());
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  EXPECT_STREQ("1", GetSelectedPath(cookies_view).c_str());

  ASSERT_TRUE(SelectByPath(cookies_view, "0"));
  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  EXPECT_STREQ("G,X", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("foo2,_Cookies,__G,__X",
               GetDisplayedCookies(cookies_view).c_str());
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  EXPECT_STREQ("0", GetSelectedPath(cookies_view).c_str());

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  EXPECT_STREQ("", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("", GetDisplayedCookies(cookies_view).c_str());
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  EXPECT_EQ(0, gtk_tree_selection_count_selected_rows(cookies_view.selection_));
}

TEST_F(CookiesViewTest, RemoveDefaultSelection) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo0"), "C=1");
  monster->SetCookie(GURL("http://foo0"), "D=1");
  monster->SetCookie(GURL("http://foo1"), "B=1");
  monster->SetCookie(GURL("http://foo1"), "A=1");
  monster->SetCookie(GURL("http://foo1"), "E=1");
  monster->SetCookie(GURL("http://foo2"), "G=1");
  monster->SetCookie(GURL("http://foo2"), "X=1");
  CookiesView cookies_view(profile_.get());
  EXPECT_STREQ("foo0,_Cookies,__C,__D,"
               "foo1,_Cookies,__A,__B,__E,"
               "foo2,_Cookies,__G,__X",
               GetDisplayedCookies(cookies_view).c_str());


  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  EXPECT_STREQ("B,A,E,G,X", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("foo1,_Cookies,__A,__B,__E,"
               "foo2,_Cookies,__G,__X",
               GetDisplayedCookies(cookies_view).c_str());

  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  EXPECT_STREQ("G,X", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("foo2,_Cookies,__G,__X",
               GetDisplayedCookies(cookies_view).c_str());

  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  EXPECT_STREQ("", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("", GetDisplayedCookies(cookies_view).c_str());

  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
}
