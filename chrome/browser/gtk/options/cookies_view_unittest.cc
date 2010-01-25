// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/options/cookies_view.h"

#include <string>
#include <vector>

#include <gtk/gtk.h>

#include "base/string_util.h"
#include "chrome/browser/mock_browsing_data_local_storage_helper.h"
#include "chrome/browser/net/url_request_context_getter.h"
#include "chrome/test/testing_profile.h"
#include "net/url_request/url_request_context.h"
#include "testing/gtest/include/gtest/gtest.h"

class CookiesViewTest : public testing::Test {
 public:
  CookiesViewTest() : io_thread_(ChromeThread::IO, &message_loop_) {
  }

  virtual ~CookiesViewTest() {
  }

  virtual void SetUp() {
    profile_.reset(new TestingProfile());
    profile_->CreateRequestContext();
    mock_browsing_data_helper_ = new MockBrowsingDataLocalStorageHelper(
        profile_.get());
  }

  void CheckDetailsSensitivity(gboolean expected_cookies,
                               gboolean expected_local_storage,
                               const CookiesView& cookies_view) {
    // Cookies
    EXPECT_EQ(expected_cookies,
              GTK_WIDGET_SENSITIVE(cookies_view.cookie_name_entry_));
    EXPECT_EQ(expected_cookies,
              GTK_WIDGET_SENSITIVE(cookies_view.cookie_content_entry_));
    EXPECT_EQ(expected_cookies,
              GTK_WIDGET_SENSITIVE(cookies_view.cookie_domain_entry_));
    EXPECT_EQ(expected_cookies,
              GTK_WIDGET_SENSITIVE(cookies_view.cookie_path_entry_));
    EXPECT_EQ(expected_cookies,
              GTK_WIDGET_SENSITIVE(cookies_view.cookie_send_for_entry_));
    EXPECT_EQ(expected_cookies,
              GTK_WIDGET_SENSITIVE(cookies_view.cookie_created_entry_));
    EXPECT_EQ(expected_cookies,
              GTK_WIDGET_SENSITIVE(cookies_view.cookie_expires_entry_));
    // Local Storage
    EXPECT_EQ(expected_local_storage,
              GTK_WIDGET_SENSITIVE(cookies_view.local_storage_origin_entry_));
    EXPECT_EQ(expected_local_storage,
              GTK_WIDGET_SENSITIVE(cookies_view.local_storage_size_entry_));
    EXPECT_EQ(expected_local_storage,
              GTK_WIDGET_SENSITIVE(
                  cookies_view.local_storage_last_modified_entry_));

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

  typedef std::pair<std::vector<std::string>*, GtkTreeView*>
      DisplayedCookiesData;

  static gboolean GetDisplayedCookiesHelper(GtkTreeModel *model,
                                            GtkTreePath *path,
                                            GtkTreeIter *iter,
                                            gpointer user_data) {
    DisplayedCookiesData* data =
        reinterpret_cast<DisplayedCookiesData*>(user_data);
    gchar* title;
    gtk_tree_model_get(model, iter,
                       gtk_tree::TreeAdapter::COL_TITLE, &title,
                       -1);
    std::string str;
    if (gtk_tree_path_get_depth(path) > 1) {
      // There isn't a function to check if a row is visible, instead we have to
      // check whether the parent is expanded.
      GtkTreePath* parent_path = gtk_tree_path_copy(path);
      gtk_tree_path_up(parent_path);
      str.append(
          gtk_tree_path_get_depth(parent_path),
          gtk_tree_view_row_expanded(data->second, parent_path) ? '+' : '_');
      gtk_tree_path_free(parent_path);
    }
    str += title;
    g_free(title);
    data->first->push_back(str);
    return FALSE;
  }

  // Get the cookie names displayed in the dialog in the order they are
  // displayed, as a comma seperated string.
  // Items are prefixed with _ or + up to their indent level.
  // _ when hidden (parent is closed) and + when visible (parent is expanded).
  // Ex: EXPECT_STREQ("a,+Cookies,++Y,b,_Cookies,__X",
  //                  GetDisplayedCookies(cookies_view).c_str());
  std::string GetDisplayedCookies(const CookiesView& cookies_view) {
    GtkTreeStore* tree_store = cookies_view.cookies_tree_adapter_->tree_store();
    std::vector<std::string> parts;
    DisplayedCookiesData helper_data(&parts, GTK_TREE_VIEW(cookies_view.tree_));
    gtk_tree_model_foreach(GTK_TREE_MODEL(tree_store),
                           GetDisplayedCookiesHelper, &helper_data);
    return JoinString(parts, ',');
  }

  bool ExpandByPath(const CookiesView& cookies_view, const char* path_str) {
    GtkTreePath* path = gtk_tree_path_new_from_string(path_str);
    if (!path)
      return false;
    bool rv = gtk_tree_view_expand_row(GTK_TREE_VIEW(cookies_view.tree_), path,
                                       FALSE);
    gtk_tree_path_free(path);
    return rv;
  }

  bool SelectByPath(const CookiesView& cookies_view, const char* path_str) {
    GtkTreePath* path = gtk_tree_path_new_from_string(path_str);
    if (!path)
      return false;
    gtk_tree_selection_select_path(cookies_view.selection_, path);
    bool rv = gtk_tree_selection_path_is_selected(cookies_view.selection_,
                                                  path);
    gtk_tree_path_free(path);
    return rv;
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
  MessageLoop message_loop_;
  ChromeThread io_thread_;

  scoped_ptr<TestingProfile> profile_;
  scoped_refptr<MockBrowsingDataLocalStorageHelper> mock_browsing_data_helper_;
};

TEST_F(CookiesViewTest, Empty) {
  CookiesView cookies_view(profile_.get(), mock_browsing_data_helper_);
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  CheckDetailsSensitivity(FALSE, FALSE, cookies_view);
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
  CookiesView cookies_view(profile_.get(), mock_browsing_data_helper_);
  mock_browsing_data_helper_->AddLocalStorageSamples();
  mock_browsing_data_helper_->Notify();
  EXPECT_STREQ("foo0,_Cookies,__C,__D,"
               "foo1,_Cookies,__A,__B,__E,"
               "foo2,_Cookies,__G,__X,"
               "host1,_Local Storage,__origin1,"
               "host2,_Local Storage,__origin2",
               GetDisplayedCookies(cookies_view).c_str());
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  CheckDetailsSensitivity(FALSE, FALSE, cookies_view);
}

TEST_F(CookiesViewTest, RemoveAll) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo"), "A=1");
  monster->SetCookie(GURL("http://foo2"), "B=1");
  CookiesView cookies_view(profile_.get(), mock_browsing_data_helper_);
  mock_browsing_data_helper_->AddLocalStorageSamples();
  mock_browsing_data_helper_->Notify();

  // Reset the selection of the first row.
  gtk_tree_selection_unselect_all(cookies_view.selection_);

  {
    SCOPED_TRACE("Before removing");
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
    EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
    CheckDetailsSensitivity(FALSE, FALSE, cookies_view);
    EXPECT_STREQ("foo,_Cookies,__A,foo2,_Cookies,__B,"
                 "host1,_Local Storage,__origin1,"
                 "host2,_Local Storage,__origin2",
                 GetDisplayedCookies(cookies_view).c_str());
  }

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_all_button_));
  {
    SCOPED_TRACE("After removing");
    EXPECT_EQ(0u, monster->GetAllCookies().size());
    EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
    EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
    CheckDetailsSensitivity(FALSE, FALSE, cookies_view);
    EXPECT_STREQ("", GetDisplayedCookies(cookies_view).c_str());
    EXPECT_TRUE(mock_browsing_data_helper_->delete_all_files_called_);
  }
}

TEST_F(CookiesViewTest, RemoveAllWithDefaultSelected) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo"), "A=1");
  monster->SetCookie(GURL("http://foo2"), "B=1");
  CookiesView cookies_view(profile_.get(), mock_browsing_data_helper_);
  mock_browsing_data_helper_->AddLocalStorageSamples();
  mock_browsing_data_helper_->Notify();

  EXPECT_STREQ("0", GetSelectedPath(cookies_view).c_str());
  EXPECT_EQ(1, gtk_tree_selection_count_selected_rows(cookies_view.selection_));
  {
    SCOPED_TRACE("Before removing");
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
    CheckDetailsSensitivity(FALSE, FALSE, cookies_view);
    EXPECT_STREQ("foo,_Cookies,__A,foo2,_Cookies,__B,"
                 "host1,_Local Storage,__origin1,"
                 "host2,_Local Storage,__origin2",
                 GetDisplayedCookies(cookies_view).c_str());
  }

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_all_button_));
  {
    SCOPED_TRACE("After removing");
    EXPECT_EQ(0u, monster->GetAllCookies().size());
    EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
    EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
    CheckDetailsSensitivity(FALSE, FALSE, cookies_view);
    EXPECT_STREQ("", GetDisplayedCookies(cookies_view).c_str());
    EXPECT_EQ(0,
              gtk_tree_selection_count_selected_rows(cookies_view.selection_));
    EXPECT_TRUE(mock_browsing_data_helper_->delete_all_files_called_);
  }
}

TEST_F(CookiesViewTest, Remove) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo1"), "A=1");
  monster->SetCookie(GURL("http://foo2"), "B=1");
  monster->SetCookie(GURL("http://foo2"), "C=1");
  CookiesView cookies_view(profile_.get(), mock_browsing_data_helper_);
  mock_browsing_data_helper_->AddLocalStorageSamples();
  mock_browsing_data_helper_->Notify();

  ASSERT_TRUE(ExpandByPath(cookies_view, "1"));
  ASSERT_TRUE(SelectByPath(cookies_view, "1:0:0"));

  {
    SCOPED_TRACE("First selection");
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
    CheckDetailsSensitivity(TRUE, FALSE, cookies_view);
    EXPECT_STREQ("foo1,_Cookies,__A,foo2,+Cookies,++B,++C,"
                 "host1,_Local Storage,__origin1,"
                 "host2,_Local Storage,__origin2",
                 GetDisplayedCookies(cookies_view).c_str());
  }

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  {
    SCOPED_TRACE("First selection removed");
    EXPECT_STREQ("A,C", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("foo1,_Cookies,__A,foo2,+Cookies,++C,"
                 "host1,_Local Storage,__origin1,"
                 "host2,_Local Storage,__origin2",
                 GetDisplayedCookies(cookies_view).c_str());
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
    EXPECT_STREQ("1:0:0", GetSelectedPath(cookies_view).c_str());
    CheckDetailsSensitivity(TRUE, FALSE, cookies_view);
  }

  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  {
    SCOPED_TRACE("Second selection");
    EXPECT_STREQ("A", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("foo1,_Cookies,__A,foo2,+Cookies,"
                 "host1,_Local Storage,__origin1,"
                 "host2,_Local Storage,__origin2",
                 GetDisplayedCookies(cookies_view).c_str());
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
    EXPECT_STREQ("1:0", GetSelectedPath(cookies_view).c_str());
    CheckDetailsSensitivity(FALSE, FALSE, cookies_view);
  }

  ASSERT_TRUE(ExpandByPath(cookies_view, "0"));
  EXPECT_STREQ("foo1,+Cookies,++A,foo2,+Cookies,"
               "host1,_Local Storage,__origin1,"
               "host2,_Local Storage,__origin2",
               GetDisplayedCookies(cookies_view).c_str());
  ASSERT_TRUE(SelectByPath(cookies_view, "0:0:0"));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  {
    SCOPED_TRACE("Second selection removed");
    EXPECT_EQ(0u, monster->GetAllCookies().size());
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
    EXPECT_STREQ("0:0", GetSelectedPath(cookies_view).c_str());
    CheckDetailsSensitivity(FALSE, FALSE, cookies_view);
    EXPECT_STREQ("foo1,+Cookies,foo2,+Cookies,"
                 "host1,_Local Storage,__origin1,"
                 "host2,_Local Storage,__origin2",
                 GetDisplayedCookies(cookies_view).c_str());
  }

  ASSERT_TRUE(ExpandByPath(cookies_view, "2"));
  EXPECT_STREQ("foo1,+Cookies,foo2,+Cookies,"
               "host1,+Local Storage,++origin1,"
               "host2,_Local Storage,__origin2",
               GetDisplayedCookies(cookies_view).c_str());
  ASSERT_TRUE(SelectByPath(cookies_view, "2:0:0"));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  {
    SCOPED_TRACE("Third selection removed");
    EXPECT_EQ(0u, monster->GetAllCookies().size());
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
    EXPECT_STREQ("2:0", GetSelectedPath(cookies_view).c_str());
    CheckDetailsSensitivity(FALSE, FALSE, cookies_view);
    EXPECT_STREQ("foo1,+Cookies,foo2,+Cookies,"
                 "host1,+Local Storage,"
                 "host2,_Local Storage,__origin2",
                 GetDisplayedCookies(cookies_view).c_str());
    EXPECT_TRUE(mock_browsing_data_helper_->last_deleted_file_ ==
                FilePath(FILE_PATH_LITERAL("file1")));
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
  CookiesView cookies_view(profile_.get(), mock_browsing_data_helper_);
  mock_browsing_data_helper_->AddLocalStorageSamples();
  mock_browsing_data_helper_->Notify();

  EXPECT_STREQ("foo0,_Cookies,__C,__D,"
               "foo1,_Cookies,__A,__B,__E,"
               "foo2,_Cookies,__G,__X,"
               "host1,_Local Storage,__origin1,"
               "host2,_Local Storage,__origin2",
               GetDisplayedCookies(cookies_view).c_str());

  ASSERT_TRUE(ExpandByPath(cookies_view, "1"));
  EXPECT_STREQ("foo0,_Cookies,__C,__D,"
               "foo1,+Cookies,++A,++B,++E,"
               "foo2,_Cookies,__G,__X,"
               "host1,_Local Storage,__origin1,"
               "host2,_Local Storage,__origin2",
               GetDisplayedCookies(cookies_view).c_str());
  ASSERT_TRUE(SelectByPath(cookies_view, "1:0"));

  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  EXPECT_STREQ("C,D,G,X", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("foo0,_Cookies,__C,__D,"
               "foo1,"
               "foo2,_Cookies,__G,__X,"
               "host1,_Local Storage,__origin1,"
               "host2,_Local Storage,__origin2",
               GetDisplayedCookies(cookies_view).c_str());
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  EXPECT_STREQ("1", GetSelectedPath(cookies_view).c_str());

  ASSERT_TRUE(ExpandByPath(cookies_view, "0"));
  EXPECT_STREQ("foo0,+Cookies,++C,++D,"
               "foo1,"
               "foo2,_Cookies,__G,__X,"
               "host1,_Local Storage,__origin1,"
               "host2,_Local Storage,__origin2",
               GetDisplayedCookies(cookies_view).c_str());
  ASSERT_TRUE(SelectByPath(cookies_view, "0:0"));
  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  EXPECT_STREQ("G,X", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("foo0,"
               "foo1,"
               "foo2,_Cookies,__G,__X,"
               "host1,_Local Storage,__origin1,"
               "host2,_Local Storage,__origin2",
               GetDisplayedCookies(cookies_view).c_str());
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  EXPECT_STREQ("0", GetSelectedPath(cookies_view).c_str());

  ASSERT_TRUE(ExpandByPath(cookies_view, "2"));
  EXPECT_STREQ("foo0,"
               "foo1,"
               "foo2,+Cookies,++G,++X,"
               "host1,_Local Storage,__origin1,"
               "host2,_Local Storage,__origin2",
               GetDisplayedCookies(cookies_view).c_str());
  ASSERT_TRUE(SelectByPath(cookies_view, "2:0"));
  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  EXPECT_STREQ("", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("foo0,"
               "foo1,"
               "foo2,"
               "host1,_Local Storage,__origin1,"
               "host2,_Local Storage,__origin2",
               GetDisplayedCookies(cookies_view).c_str());
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  EXPECT_STREQ("2", GetSelectedPath(cookies_view).c_str());

  ASSERT_TRUE(ExpandByPath(cookies_view, "3"));
  EXPECT_STREQ("foo0,"
               "foo1,"
               "foo2,"
               "host1,+Local Storage,++origin1,"
               "host2,_Local Storage,__origin2",
               GetDisplayedCookies(cookies_view).c_str());
  ASSERT_TRUE(SelectByPath(cookies_view, "3:0"));
  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  EXPECT_STREQ("", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("foo0,"
               "foo1,"
               "foo2,"
               "host1,"
               "host2,_Local Storage,__origin2",
               GetDisplayedCookies(cookies_view).c_str());
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  EXPECT_STREQ("3", GetSelectedPath(cookies_view).c_str());
  EXPECT_TRUE(mock_browsing_data_helper_->last_deleted_file_ ==
              FilePath(FILE_PATH_LITERAL("file1")));
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
  CookiesView cookies_view(profile_.get(), mock_browsing_data_helper_);
  mock_browsing_data_helper_->AddLocalStorageSamples();
  mock_browsing_data_helper_->Notify();

  EXPECT_STREQ("foo0,_Cookies,__C,__D,"
               "foo1,_Cookies,__A,__B,__E,"
               "foo2,_Cookies,__G,__X,"
               "host1,_Local Storage,__origin1,"
               "host2,_Local Storage,__origin2",
               GetDisplayedCookies(cookies_view).c_str());

  ASSERT_TRUE(SelectByPath(cookies_view, "1"));

  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  EXPECT_STREQ("C,D,G,X", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("foo0,_Cookies,__C,__D,"
               "foo2,_Cookies,__G,__X,"
               "host1,_Local Storage,__origin1,"
               "host2,_Local Storage,__origin2",
               GetDisplayedCookies(cookies_view).c_str());
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  EXPECT_STREQ("1", GetSelectedPath(cookies_view).c_str());

  ASSERT_TRUE(SelectByPath(cookies_view, "0"));
  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  EXPECT_STREQ("G,X", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("foo2,_Cookies,__G,__X,"
               "host1,_Local Storage,__origin1,"
               "host2,_Local Storage,__origin2",
               GetDisplayedCookies(cookies_view).c_str());
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  EXPECT_STREQ("0", GetSelectedPath(cookies_view).c_str());

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  EXPECT_STREQ("", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("host1,_Local Storage,__origin1,"
               "host2,_Local Storage,__origin2",
               GetDisplayedCookies(cookies_view).c_str());
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  EXPECT_STREQ("0", GetSelectedPath(cookies_view).c_str());

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  EXPECT_STREQ("", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("host2,_Local Storage,__origin2",
               GetDisplayedCookies(cookies_view).c_str());
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  EXPECT_TRUE(mock_browsing_data_helper_->last_deleted_file_ ==
        FilePath(FILE_PATH_LITERAL("file1")));
  EXPECT_STREQ("0", GetSelectedPath(cookies_view).c_str());

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));
  EXPECT_STREQ("", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("",
               GetDisplayedCookies(cookies_view).c_str());
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  EXPECT_TRUE(mock_browsing_data_helper_->last_deleted_file_ ==
        FilePath(FILE_PATH_LITERAL("file2")));

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
  CookiesView cookies_view(profile_.get(), mock_browsing_data_helper_);
  mock_browsing_data_helper_->AddLocalStorageSamples();
  mock_browsing_data_helper_->Notify();

  EXPECT_STREQ("foo0,_Cookies,__C,__D,"
               "foo1,_Cookies,__A,__B,__E,"
               "foo2,_Cookies,__G,__X,"
               "host1,_Local Storage,__origin1,"
               "host2,_Local Storage,__origin2",
               GetDisplayedCookies(cookies_view).c_str());


  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  EXPECT_STREQ("B,A,E,G,X", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("foo1,_Cookies,__A,__B,__E,"
               "foo2,_Cookies,__G,__X,"
               "host1,_Local Storage,__origin1,"
               "host2,_Local Storage,__origin2",
               GetDisplayedCookies(cookies_view).c_str());

  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  EXPECT_STREQ("G,X", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("foo2,_Cookies,__G,__X,"
               "host1,_Local Storage,__origin1,"
               "host2,_Local Storage,__origin2",
               GetDisplayedCookies(cookies_view).c_str());

  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  EXPECT_STREQ("", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("host1,_Local Storage,__origin1,"
               "host2,_Local Storage,__origin2",
               GetDisplayedCookies(cookies_view).c_str());

  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  EXPECT_STREQ("", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("host2,_Local Storage,__origin2",
               GetDisplayedCookies(cookies_view).c_str());

  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  EXPECT_STREQ("", GetMonsterCookies(monster).c_str());
  EXPECT_STREQ("", GetDisplayedCookies(cookies_view).c_str());

  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
}

TEST_F(CookiesViewTest, Filter) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo0"), "C=1");
  monster->SetCookie(GURL("http://bar0"), "D=1");
  monster->SetCookie(GURL("http://foo1"), "B=1");
  monster->SetCookie(GURL("http://bar1"), "A=1");
  CookiesView cookies_view(profile_.get(), mock_browsing_data_helper_);
  mock_browsing_data_helper_->AddLocalStorageSamples();
  mock_browsing_data_helper_->Notify();

  EXPECT_STREQ("bar0,_Cookies,__D,"
               "bar1,_Cookies,__A,"
               "foo0,_Cookies,__C,"
               "foo1,_Cookies,__B,"
               "host1,_Local Storage,__origin1,"
               "host2,_Local Storage,__origin2",
               GetDisplayedCookies(cookies_view).c_str());

  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.filter_clear_button_));

  gtk_entry_set_text(GTK_ENTRY(cookies_view.filter_entry_), "bar");
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.filter_clear_button_));
  // Entering text doesn't immediately filter the results.
  EXPECT_STREQ("bar0,_Cookies,__D,"
               "bar1,_Cookies,__A,"
               "foo0,_Cookies,__C,"
               "foo1,_Cookies,__B,"
               "host1,_Local Storage,__origin1,"
               "host2,_Local Storage,__origin2",
               GetDisplayedCookies(cookies_view).c_str());

  // Results are filtered immediately if you activate (hit enter in the entry).
  gtk_widget_activate(cookies_view.filter_entry_);
  EXPECT_STREQ("bar0,_Cookies,__D,"
               "bar1,_Cookies,__A",
               GetDisplayedCookies(cookies_view).c_str());

  gtk_button_clicked(GTK_BUTTON(cookies_view.filter_clear_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.filter_clear_button_));
  EXPECT_STREQ("", gtk_entry_get_text(GTK_ENTRY(cookies_view.filter_entry_)));
  EXPECT_STREQ("bar0,_Cookies,__D,"
               "bar1,_Cookies,__A,"
               "foo0,_Cookies,__C,"
               "foo1,_Cookies,__B,"
               "host1,_Local Storage,__origin1,"
               "host2,_Local Storage,__origin2",
               GetDisplayedCookies(cookies_view).c_str());

  gtk_entry_set_text(GTK_ENTRY(cookies_view.filter_entry_), "hos");
  gtk_widget_activate(cookies_view.filter_entry_);
  EXPECT_STREQ("host1,_Local Storage,__origin1,"
               "host2,_Local Storage,__origin2",
               GetDisplayedCookies(cookies_view).c_str());
}

TEST_F(CookiesViewTest, FilterRemoveAll) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo0"), "C=1");
  monster->SetCookie(GURL("http://bar0"), "D=1");
  monster->SetCookie(GURL("http://foo1"), "B=1");
  monster->SetCookie(GURL("http://bar1"), "A=1");
  CookiesView cookies_view(profile_.get(), mock_browsing_data_helper_);
  mock_browsing_data_helper_->AddLocalStorageSamples();
  mock_browsing_data_helper_->Notify();

  EXPECT_STREQ("bar0,_Cookies,__D,"
               "bar1,_Cookies,__A,"
               "foo0,_Cookies,__C,"
               "foo1,_Cookies,__B,"
               "host1,_Local Storage,__origin1,"
               "host2,_Local Storage,__origin2",
               GetDisplayedCookies(cookies_view).c_str());

  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.filter_clear_button_));

  gtk_entry_set_text(GTK_ENTRY(cookies_view.filter_entry_), "bar");
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.filter_clear_button_));
  // Entering text doesn't immediately filter the results.
  EXPECT_STREQ("bar0,_Cookies,__D,"
               "bar1,_Cookies,__A,"
               "foo0,_Cookies,__C,"
               "foo1,_Cookies,__B,"
               "host1,_Local Storage,__origin1,"
               "host2,_Local Storage,__origin2",
               GetDisplayedCookies(cookies_view).c_str());

  // Results are filtered immediately if you activate (hit enter in the entry).
  gtk_widget_activate(cookies_view.filter_entry_);
  EXPECT_STREQ("bar0,_Cookies,__D,"
               "bar1,_Cookies,__A",
               GetDisplayedCookies(cookies_view).c_str());
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_all_button_));

  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
  EXPECT_STREQ("",
               GetDisplayedCookies(cookies_view).c_str());
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.filter_clear_button_));

  gtk_button_clicked(GTK_BUTTON(cookies_view.filter_clear_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.filter_clear_button_));
  EXPECT_STREQ("", gtk_entry_get_text(GTK_ENTRY(cookies_view.filter_entry_)));
  EXPECT_STREQ("foo0,_Cookies,__C,"
               "foo1,_Cookies,__B,"
               "host1,_Local Storage,__origin1,"
               "host2,_Local Storage,__origin2",
               GetDisplayedCookies(cookies_view).c_str());
}

TEST_F(CookiesViewTest, FilterRemove) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo0"), "C=1");
  monster->SetCookie(GURL("http://bar0"), "D=1");
  monster->SetCookie(GURL("http://foo1"), "B=1");
  monster->SetCookie(GURL("http://bar1"), "A=1");
  monster->SetCookie(GURL("http://bar1"), "E=1");
  CookiesView cookies_view(profile_.get(), mock_browsing_data_helper_);
  mock_browsing_data_helper_->AddLocalStorageSamples();
  mock_browsing_data_helper_->Notify();

  EXPECT_STREQ("bar0,_Cookies,__D,"
               "bar1,_Cookies,__A,__E,"
               "foo0,_Cookies,__C,"
               "foo1,_Cookies,__B,"
               "host1,_Local Storage,__origin1,"
               "host2,_Local Storage,__origin2",
               GetDisplayedCookies(cookies_view).c_str());
  EXPECT_STREQ("D,A,E,C,B", GetMonsterCookies(monster).c_str());

  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.filter_clear_button_));

  gtk_entry_set_text(GTK_ENTRY(cookies_view.filter_entry_), "bar");
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.filter_clear_button_));
  // Entering text doesn't immediately filter the results.
  EXPECT_STREQ("bar0,_Cookies,__D,"
               "bar1,_Cookies,__A,__E,"
               "foo0,_Cookies,__C,"
               "foo1,_Cookies,__B,"
               "host1,_Local Storage,__origin1,"
               "host2,_Local Storage,__origin2",
               GetDisplayedCookies(cookies_view).c_str());

  // Results are filtered immediately if you activate (hit enter in the entry).
  gtk_widget_activate(cookies_view.filter_entry_);
  EXPECT_STREQ("bar0,_Cookies,__D,"
               "bar1,_Cookies,__A,__E",
               GetDisplayedCookies(cookies_view).c_str());

  ASSERT_TRUE(ExpandByPath(cookies_view, "1"));
  EXPECT_STREQ("bar0,_Cookies,__D,"
               "bar1,+Cookies,++A,++E",
               GetDisplayedCookies(cookies_view).c_str());
  ASSERT_TRUE(SelectByPath(cookies_view, "1:0:0"));

  {
    SCOPED_TRACE("First selection");
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
    CheckDetailsSensitivity(TRUE, FALSE, cookies_view);
  }

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  {
    SCOPED_TRACE("First selection removed");
    EXPECT_STREQ("D,E,C,B", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("bar0,_Cookies,__D,"
                 "bar1,+Cookies,++E",
                 GetDisplayedCookies(cookies_view).c_str());
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
    EXPECT_STREQ("1:0:0", GetSelectedPath(cookies_view).c_str());
    CheckDetailsSensitivity(TRUE, FALSE, cookies_view);
  }

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  {
    SCOPED_TRACE("Second selection");
    EXPECT_STREQ("D,C,B", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("bar0,_Cookies,__D,"
                 "bar1,+Cookies",
                 GetDisplayedCookies(cookies_view).c_str());
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
    EXPECT_STREQ("1:0", GetSelectedPath(cookies_view).c_str());
    CheckDetailsSensitivity(FALSE, FALSE, cookies_view);
  }

  ASSERT_TRUE(ExpandByPath(cookies_view, "0"));
  ASSERT_TRUE(SelectByPath(cookies_view, "0:0:0"));
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  {
    SCOPED_TRACE("Second selection removed");
    EXPECT_STREQ("C,B", GetMonsterCookies(monster).c_str());
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
    EXPECT_STREQ("0:0", GetSelectedPath(cookies_view).c_str());
    CheckDetailsSensitivity(FALSE, FALSE, cookies_view);
    EXPECT_STREQ("bar0,+Cookies,"
                 "bar1,+Cookies",
                 GetDisplayedCookies(cookies_view).c_str());
  }

  gtk_button_clicked(GTK_BUTTON(cookies_view.filter_clear_button_));
  EXPECT_EQ(FALSE, GTK_WIDGET_SENSITIVE(cookies_view.filter_clear_button_));
  EXPECT_STREQ("", gtk_entry_get_text(GTK_ENTRY(cookies_view.filter_entry_)));
  EXPECT_STREQ("foo0,_Cookies,__C,"
               "foo1,_Cookies,__B,"
               "host1,_Local Storage,__origin1,"
               "host2,_Local Storage,__origin2",
               GetDisplayedCookies(cookies_view).c_str());

  gtk_entry_set_text(GTK_ENTRY(cookies_view.filter_entry_), "hos");
  EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.filter_clear_button_));
  // Entering text doesn't immediately filter the results.
  EXPECT_STREQ("foo0,_Cookies,__C,"
               "foo1,_Cookies,__B,"
               "host1,_Local Storage,__origin1,"
               "host2,_Local Storage,__origin2",
               GetDisplayedCookies(cookies_view).c_str());

  // Results are filtered immediately if you activate (hit enter in the entry).
  gtk_widget_activate(cookies_view.filter_entry_);
  EXPECT_STREQ("host1,_Local Storage,__origin1,"
               "host2,_Local Storage,__origin2",
               GetDisplayedCookies(cookies_view).c_str());

  ASSERT_TRUE(ExpandByPath(cookies_view, "1"));
  EXPECT_STREQ("host1,_Local Storage,__origin1,"
               "host2,+Local Storage,++origin2",
               GetDisplayedCookies(cookies_view).c_str());
  ASSERT_TRUE(SelectByPath(cookies_view, "1:0:0"));

  {
    SCOPED_TRACE("First selection");
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
    CheckDetailsSensitivity(FALSE, TRUE, cookies_view);
  }

  gtk_button_clicked(GTK_BUTTON(cookies_view.remove_button_));

  {
    SCOPED_TRACE("First selection removed");
    EXPECT_STREQ("C,B", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("host1,_Local Storage,__origin1,"
           "host2,+Local Storage",
                 GetDisplayedCookies(cookies_view).c_str());
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_all_button_));
    EXPECT_EQ(TRUE, GTK_WIDGET_SENSITIVE(cookies_view.remove_button_));
    EXPECT_STREQ("1:0", GetSelectedPath(cookies_view).c_str());
    CheckDetailsSensitivity(FALSE, FALSE, cookies_view);
  }
}
