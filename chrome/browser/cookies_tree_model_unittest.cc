// Copyright (c) 2009-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cookies_tree_model.h"

#include <string>

#include "app/l10n_util.h"
#include "chrome/browser/net/url_request_context_getter.h"
#include "chrome/test/testing_profile.h"
#include "net/url_request/url_request_context.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace {

class CookiesTreeModelTest : public testing::Test {
 public:
  CookiesTreeModelTest() : io_thread_(ChromeThread::IO, &message_loop_) {
  }

  virtual ~CookiesTreeModelTest() {
  }

  virtual void SetUp() {
    profile_.reset(new TestingProfile());
    profile_->CreateRequestContext();
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

  std::string GetCookiesOfChildren(const CookieTreeNode* node) {
    if (node->GetChildCount()) {
      std::string retval;
      for (int i = 0; i < node->GetChildCount(); ++i) {
        retval += GetCookiesOfChildren(node->GetChild(i));
      }
      return retval;
    } else {
      if (node->GetDetailedInfo().node_type ==
          CookieTreeNode::DetailedInfo::TYPE_COOKIE)
        return node->GetDetailedInfo().cookie->second.Name() + ",";
      else
        return "";
    }
  }
  // Get the cookie names displayed in the view (if we had one) in the order
  // they are displayed, as a comma seperated string.
  // Ex: EXPECT_STREQ("X,Y", GetDisplayedCookies(cookies_view).c_str());
  std::string GetDisplayedCookies(CookiesTreeModel* cookies_model) {
    CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(
        cookies_model->GetRoot());
    std::string retval = GetCookiesOfChildren(root);
    if (retval.length() && retval[retval.length() - 1] == ',')
      retval.erase(retval.length() - 1);
    return retval;
  }

  // do not call on the root
  void DeleteCookie(CookieTreeNode* node) {
    node->DeleteStoredObjects();
    // find the parent and index
    CookieTreeNode* parent_node = node->GetParent();
    DCHECK(parent_node);
    int ct_node_index = parent_node->IndexOfChild(node);
    delete parent_node->GetModel()->Remove(parent_node, ct_node_index);
  }
 protected:
  MessageLoop message_loop_;
  ChromeThread io_thread_;

  scoped_ptr<TestingProfile> profile_;
};

TEST_F(CookiesTreeModelTest, RemoveAll) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo"), "A=1");
  monster->SetCookie(GURL("http://foo2"), "B=1");
  CookiesTreeModel cookies_model(profile_.get());

  // Reset the selection of the first row.
  {
    SCOPED_TRACE("Before removing");
    EXPECT_EQ(GetMonsterCookies(monster), GetDisplayedCookies(&cookies_model));
  }

  cookies_model.DeleteAllCookies();
  {
    SCOPED_TRACE("After removing");
    EXPECT_EQ(1, cookies_model.GetRoot()->GetTotalNodeCount());
    EXPECT_EQ(0, cookies_model.GetRoot()->GetChildCount());
    EXPECT_EQ(std::string(""), GetMonsterCookies(monster));
    EXPECT_EQ(GetMonsterCookies(monster), GetDisplayedCookies(&cookies_model));
  }
}

TEST_F(CookiesTreeModelTest, Remove) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo1"), "A=1");
  monster->SetCookie(GURL("http://foo2"), "B=1");
  monster->SetCookie(GURL("http://foo3"), "C=1");
  CookiesTreeModel cookies_model(profile_.get());

  {
    SCOPED_TRACE("Initial State 3 cookies");
    // 10 because there's the root, then foo1 -> cookies -> a,
    // foo2 -> cookies -> b, foo3 -> cookies -> c
    EXPECT_EQ(10, cookies_model.GetRoot()->GetTotalNodeCount());
  }
  DeleteCookie(cookies_model.GetRoot()->GetChild(0));
  {
    SCOPED_TRACE("First origin removed");
    EXPECT_STREQ("B,C", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("B,C", GetDisplayedCookies(&cookies_model).c_str());
    EXPECT_EQ(7, cookies_model.GetRoot()->GetTotalNodeCount());
  }
}

TEST_F(CookiesTreeModelTest, RemoveCookiesNode) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo1"), "A=1");
  monster->SetCookie(GURL("http://foo2"), "B=1");
  monster->SetCookie(GURL("http://foo3"), "C=1");
  CookiesTreeModel cookies_model(profile_.get());

  {
    SCOPED_TRACE("Initial State 3 cookies");
    // 10 because there's the root, then foo1 -> cookies -> a,
    // foo2 -> cookies -> b, foo3 -> cookies -> c
    EXPECT_EQ(10, cookies_model.GetRoot()->GetTotalNodeCount());
  }
  DeleteCookie(cookies_model.GetRoot()->GetChild(0)->GetChild(0));
  {
    SCOPED_TRACE("First origin removed");
    EXPECT_STREQ("B,C", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("B,C", GetDisplayedCookies(&cookies_model).c_str());
    // 8 because in this case, the origin remains, although the COOKIES
    // node beneath it has been deleted. So, we have
    // root -> foo1 -> cookies -> a, foo2, foo3 -> cookies -> c
    EXPECT_EQ(8, cookies_model.GetRoot()->GetTotalNodeCount());
  }
}

TEST_F(CookiesTreeModelTest, RemoveCookieNode) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo1"), "A=1");
  monster->SetCookie(GURL("http://foo2"), "B=1");
  monster->SetCookie(GURL("http://foo3"), "C=1");
  CookiesTreeModel cookies_model(profile_.get());

  {
    SCOPED_TRACE("Initial State 3 cookies");
    // 10 because there's the root, then foo1 -> cookies -> a,
    // foo2 -> cookies -> b, foo3 -> cookies -> c
    EXPECT_EQ(10, cookies_model.GetRoot()->GetTotalNodeCount());
  }
  DeleteCookie(cookies_model.GetRoot()->GetChild(1)->GetChild(0));
  {
    SCOPED_TRACE("Second origin COOKIES node removed");
    EXPECT_STREQ("A,C", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("A,C", GetDisplayedCookies(&cookies_model).c_str());
    // 8 because in this case, the origin remains, although the COOKIES
    // node beneath it has been deleted. So, we have
    // root -> foo1 -> cookies -> a, foo2, foo3 -> cookies -> c
    EXPECT_EQ(8, cookies_model.GetRoot()->GetTotalNodeCount());
  }
}

TEST_F(CookiesTreeModelTest, RemoveSingleCookieNode) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo1"), "A=1");
  monster->SetCookie(GURL("http://foo2"), "B=1");
  monster->SetCookie(GURL("http://foo3"), "C=1");
  monster->SetCookie(GURL("http://foo3"), "D=1");
  CookiesTreeModel cookies_model(profile_.get());

  {
    SCOPED_TRACE("Initial State 4 cookies");
    // 11 because there's the root, then foo1 -> cookies -> a,
    // foo2 -> cookies -> b, foo3 -> cookies -> c,d
    EXPECT_EQ(11, cookies_model.GetRoot()->GetTotalNodeCount());
    EXPECT_STREQ("A,B,C,D", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("A,B,C,D", GetDisplayedCookies(&cookies_model).c_str());
  }
  DeleteCookie(cookies_model.GetRoot()->GetChild(2));
  {
    SCOPED_TRACE("Third origin removed");
    EXPECT_STREQ("A,B", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("A,B", GetDisplayedCookies(&cookies_model).c_str());
    EXPECT_EQ(7, cookies_model.GetRoot()->GetTotalNodeCount());
  }
}

TEST_F(CookiesTreeModelTest, RemoveSingleCookieNodeOf3) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo1"), "A=1");
  monster->SetCookie(GURL("http://foo2"), "B=1");
  monster->SetCookie(GURL("http://foo3"), "C=1");
  monster->SetCookie(GURL("http://foo3"), "D=1");
  monster->SetCookie(GURL("http://foo3"), "E=1");
  CookiesTreeModel cookies_model(profile_.get());

  {
    SCOPED_TRACE("Initial State 5 cookies");
    // 11 because there's the root, then foo1 -> cookies -> a,
    // foo2 -> cookies -> b, foo3 -> cookies -> c,d,e
    EXPECT_EQ(12, cookies_model.GetRoot()->GetTotalNodeCount());
    EXPECT_STREQ("A,B,C,D,E", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("A,B,C,D,E", GetDisplayedCookies(&cookies_model).c_str());
  }
  DeleteCookie(cookies_model.GetRoot()->GetChild(2)->GetChild(0)->
      GetChild(1));
  {
    SCOPED_TRACE("Middle cookie in third origin removed");
    EXPECT_STREQ("A,B,C,E", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("A,B,C,E", GetDisplayedCookies(&cookies_model).c_str());
    EXPECT_EQ(11, cookies_model.GetRoot()->GetTotalNodeCount());
  }
}

TEST_F(CookiesTreeModelTest, RemoveSecondOrigin) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo1"), "A=1");
  monster->SetCookie(GURL("http://foo2"), "B=1");
  monster->SetCookie(GURL("http://foo3"), "C=1");
  monster->SetCookie(GURL("http://foo3"), "D=1");
  monster->SetCookie(GURL("http://foo3"), "E=1");
  CookiesTreeModel cookies_model(profile_.get());

  {
    SCOPED_TRACE("Initial State 5 cookies");
    // 11 because there's the root, then foo1 -> cookies -> a,
    // foo2 -> cookies -> b, foo3 -> cookies -> c,d,e
    EXPECT_EQ(12, cookies_model.GetRoot()->GetTotalNodeCount());
    EXPECT_STREQ("A,B,C,D,E", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("A,B,C,D,E", GetDisplayedCookies(&cookies_model).c_str());
  }
  DeleteCookie(cookies_model.GetRoot()->GetChild(1));
  {
    SCOPED_TRACE("Second origin removed");
    EXPECT_STREQ("A,C,D,E", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("A,C,D,E", GetDisplayedCookies(&cookies_model).c_str());
    // Left with root -> foo1 -> cookies -> a, foo3 -> cookies -> c,d,e
    EXPECT_EQ(9, cookies_model.GetRoot()->GetTotalNodeCount());
  }
}

TEST_F(CookiesTreeModelTest, OriginOrdering) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://a.foo2.com"), "A=1");
  monster->SetCookie(GURL("http://foo2.com"), "B=1");
  monster->SetCookie(GURL("http://b.foo1.com"), "C=1");
  monster->SetCookie(GURL("http://foo4.com"), "D=1; domain=.foo4.com;"
      " path=/;");  // Leading dot on the foo4
  monster->SetCookie(GURL("http://a.foo1.com"), "E=1");
  monster->SetCookie(GURL("http://foo1.com"), "F=1");
  monster->SetCookie(GURL("http://foo3.com"), "G=1");
  monster->SetCookie(GURL("http://foo4.com"), "H=1");

  CookiesTreeModel cookies_model(profile_.get());

  {
    SCOPED_TRACE("Initial State 8 cookies");
    // D starts with a ., CookieMonster orders that lexicographically first
    EXPECT_STREQ("D,E,A,C,F,B,G,H", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("F,E,C,B,A,G,D,H",
        GetDisplayedCookies(&cookies_model).c_str());
  }
  DeleteCookie(cookies_model.GetRoot()->GetChild(1));  // Delete "E"
  {
    SCOPED_TRACE("Second origin removed");
    EXPECT_STREQ("D,A,C,F,B,G,H", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("F,C,B,A,G,D,H", GetDisplayedCookies(&cookies_model).c_str());
  }
}



}  // namespace
