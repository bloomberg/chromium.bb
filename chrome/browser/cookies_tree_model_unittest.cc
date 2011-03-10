// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cookies_tree_model.h"

#include <string>

#include "chrome/browser/content_settings/stub_settings_observer.h"
#include "chrome/browser/mock_browsing_data_appcache_helper.h"
#include "chrome/browser/mock_browsing_data_database_helper.h"
#include "chrome/browser/mock_browsing_data_indexed_db_helper.h"
#include "chrome/browser/mock_browsing_data_local_storage_helper.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/test/testing_profile.h"
#include "content/common/notification_details.h"
#include "content/common/notification_type.h"
#include "net/url_request/url_request_context.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace {

class CookiesTreeModelTest : public testing::Test {
 public:
  CookiesTreeModelTest() : ui_thread_(BrowserThread::UI, &message_loop_),
                           io_thread_(BrowserThread::IO, &message_loop_) {
  }

  virtual ~CookiesTreeModelTest() {
  }

  virtual void SetUp() {
    profile_.reset(new TestingProfile());
    profile_->CreateRequestContext();
    mock_browsing_data_database_helper_ =
      new MockBrowsingDataDatabaseHelper(profile_.get());
    mock_browsing_data_local_storage_helper_ =
      new MockBrowsingDataLocalStorageHelper(profile_.get());
    mock_browsing_data_session_storage_helper_ =
      new MockBrowsingDataLocalStorageHelper(profile_.get());
    mock_browsing_data_appcache_helper_ =
      new MockBrowsingDataAppCacheHelper(profile_.get());
    mock_browsing_data_indexed_db_helper_ =
      new MockBrowsingDataIndexedDBHelper(profile_.get());
  }

  CookiesTreeModel* CreateCookiesTreeModelWithInitialSample() {
    net::CookieMonster* monster = profile_->GetCookieMonster();
    monster->SetCookie(GURL("http://foo1"), "A=1");
    monster->SetCookie(GURL("http://foo2"), "B=1");
    monster->SetCookie(GURL("http://foo3"), "C=1");
    CookiesTreeModel* cookies_model = new CookiesTreeModel(
        monster, mock_browsing_data_database_helper_,
        mock_browsing_data_local_storage_helper_,
        mock_browsing_data_session_storage_helper_,
        mock_browsing_data_appcache_helper_,
        mock_browsing_data_indexed_db_helper_);
    mock_browsing_data_database_helper_->AddDatabaseSamples();
    mock_browsing_data_database_helper_->Notify();
    mock_browsing_data_local_storage_helper_->AddLocalStorageSamples();
    mock_browsing_data_local_storage_helper_->Notify();
    mock_browsing_data_session_storage_helper_->AddLocalStorageSamples();
    mock_browsing_data_session_storage_helper_->Notify();
    mock_browsing_data_indexed_db_helper_->AddIndexedDBSamples();
    mock_browsing_data_indexed_db_helper_->Notify();
    {
      SCOPED_TRACE("Initial State 3 cookies, 2 databases, 2 local storages, "
                   "2 session storages, 2 indexed DBs");
      // 32 because there's the root, then foo1 -> cookies -> a,
      // foo2 -> cookies -> b, foo3 -> cookies -> c,
      // dbhost1 -> database -> db1, dbhost2 -> database -> db2,
      // host1 -> localstorage -> http://host1:1/,
      // host2 -> localstorage -> http://host2:2/.
      // host1 -> sessionstorage -> http://host1:1/,
      // host2 -> sessionstorage -> http://host2:2/,
      // idbhost1 -> indexeddb -> http://idbhost1:1/,
      // idbhost2 -> indexeddb -> http://idbhost2:2/.
      EXPECT_EQ(32, cookies_model->GetRoot()->GetTotalNodeCount());
      EXPECT_EQ("db1,db2", GetDisplayedDatabases(cookies_model));
      EXPECT_EQ("http://host1:1/,http://host2:2/",
                GetDisplayedLocalStorages(cookies_model));
      EXPECT_EQ("http://host1:1/,http://host2:2/",
                GetDisplayedSessionStorages(cookies_model));
      EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
                GetDisplayedIndexedDBs(cookies_model));
    }
    return cookies_model;
  }

  // Get the cookie names in the cookie list, as a comma seperated string.
  // (Note that the CookieMonster cookie list is sorted by domain.)
  // Ex:
  //   monster->SetCookie(GURL("http://b"), "X=1")
  //   monster->SetCookie(GURL("http://a"), "Y=1")
  //   EXPECT_STREQ("Y,X", GetMonsterCookies(monster).c_str());
  std::string GetMonsterCookies(net::CookieMonster* monster) {
    std::vector<std::string> parts;
    net::CookieList cookie_list = monster->GetAllCookies();
    for (size_t i = 0; i < cookie_list.size(); ++i)
      parts.push_back(cookie_list[i].Name());
    return JoinString(parts, ',');
  }

  std::string GetNodesOfChildren(
      const CookieTreeNode* node,
      CookieTreeNode::DetailedInfo::NodeType node_type) {
    if (node->child_count()) {
      std::string retval;
      for (int i = 0; i < node->child_count(); ++i) {
        retval += GetNodesOfChildren(node->GetChild(i), node_type);
      }
      return retval;
    } else {
      if (node->GetDetailedInfo().node_type == node_type) {
        switch (node_type) {
          case CookieTreeNode::DetailedInfo::TYPE_SESSION_STORAGE:
            return node->GetDetailedInfo().session_storage_info->origin + ",";
          case CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGE:
            return node->GetDetailedInfo().local_storage_info->origin + ",";
          case CookieTreeNode::DetailedInfo::TYPE_DATABASE:
            return node->GetDetailedInfo().database_info->database_name + ",";
          case CookieTreeNode::DetailedInfo::TYPE_COOKIE:
            return node->GetDetailedInfo().cookie->Name() + ",";
          case CookieTreeNode::DetailedInfo::TYPE_APPCACHE:
            return node->GetDetailedInfo().appcache_info->manifest_url.spec() +
                   ",";
          case CookieTreeNode::DetailedInfo::TYPE_INDEXED_DB:
            return node->GetDetailedInfo().indexed_db_info->origin + ",";
          default:
            return "";
        }
      } else {
        return "";
      }
    }
  }

  std::string GetCookiesOfChildren(const CookieTreeNode* node) {
    return GetNodesOfChildren(node, CookieTreeNode::DetailedInfo::TYPE_COOKIE);
  }

  std::string GetDatabasesOfChildren(const CookieTreeNode* node) {
    return GetNodesOfChildren(node,
                              CookieTreeNode::DetailedInfo::TYPE_DATABASE);
  }

  std::string GetLocalStoragesOfChildren(const CookieTreeNode* node) {
    return GetNodesOfChildren(node,
                              CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGE);
  }

  std::string GetSessionStoragesOfChildren(const CookieTreeNode* node) {
    return GetNodesOfChildren(
        node, CookieTreeNode::DetailedInfo::TYPE_SESSION_STORAGE);
  }

  std::string GetIndexedDBsOfChildren(const CookieTreeNode* node) {
    return GetNodesOfChildren(
        node, CookieTreeNode::DetailedInfo::TYPE_INDEXED_DB);
  }

  // Get the nodes names displayed in the view (if we had one) in the order
  // they are displayed, as a comma seperated string.
  // Ex: EXPECT_STREQ("X,Y", GetDisplayedNodes(cookies_view, type).c_str());
  std::string GetDisplayedNodes(CookiesTreeModel* cookies_model,
                                CookieTreeNode::DetailedInfo::NodeType type) {
    CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(
        cookies_model->GetRoot());
    std::string retval = GetNodesOfChildren(root, type);
    if (retval.length() && retval[retval.length() - 1] == ',')
      retval.erase(retval.length() - 1);
    return retval;
  }

  std::string GetDisplayedCookies(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(cookies_model,
                             CookieTreeNode::DetailedInfo::TYPE_COOKIE);
  }

  std::string GetDisplayedDatabases(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(cookies_model,
                             CookieTreeNode::DetailedInfo::TYPE_DATABASE);
  }

  std::string GetDisplayedLocalStorages(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(cookies_model,
                             CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGE);
  }

  std::string GetDisplayedSessionStorages(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(
        cookies_model, CookieTreeNode::DetailedInfo::TYPE_SESSION_STORAGE);
  }

  std::string GetDisplayedAppCaches(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(cookies_model,
                             CookieTreeNode::DetailedInfo::TYPE_APPCACHE);
  }

  std::string GetDisplayedIndexedDBs(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(cookies_model,
                             CookieTreeNode::DetailedInfo::TYPE_INDEXED_DB);
  }

  // do not call on the root
  void DeleteStoredObjects(CookieTreeNode* node) {
    node->DeleteStoredObjects();
    // find the parent and index
    CookieTreeNode* parent_node = node->parent();
    DCHECK(parent_node);
    int ct_node_index = parent_node->GetIndexOf(node);
    delete parent_node->GetModel()->Remove(parent_node, ct_node_index);
  }
 protected:
  MessageLoop message_loop_;
  BrowserThread ui_thread_;
  BrowserThread io_thread_;

  scoped_ptr<TestingProfile> profile_;
  scoped_refptr<MockBrowsingDataDatabaseHelper>
      mock_browsing_data_database_helper_;
  scoped_refptr<MockBrowsingDataLocalStorageHelper>
      mock_browsing_data_local_storage_helper_;
  scoped_refptr<MockBrowsingDataLocalStorageHelper>
      mock_browsing_data_session_storage_helper_;
  scoped_refptr<MockBrowsingDataAppCacheHelper>
      mock_browsing_data_appcache_helper_;
  scoped_refptr<MockBrowsingDataIndexedDBHelper>
      mock_browsing_data_indexed_db_helper_;
};

TEST_F(CookiesTreeModelTest, RemoveAll) {
  scoped_ptr<CookiesTreeModel> cookies_model(
      CreateCookiesTreeModelWithInitialSample());
  net::CookieMonster* monster = profile_->GetCookieMonster();

  // Reset the selection of the first row.
  {
    SCOPED_TRACE("Before removing");
    EXPECT_EQ(GetMonsterCookies(monster),
              GetDisplayedCookies(cookies_model.get()));
    EXPECT_EQ("db1,db2",
              GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
  }

  mock_browsing_data_database_helper_->Reset();
  mock_browsing_data_local_storage_helper_->Reset();
  mock_browsing_data_session_storage_helper_->Reset();
  mock_browsing_data_indexed_db_helper_->Reset();

  cookies_model->DeleteAllStoredObjects();

  {
    SCOPED_TRACE("After removing");
    EXPECT_EQ(1, cookies_model->GetRoot()->GetTotalNodeCount());
    EXPECT_EQ(0, cookies_model->GetRoot()->child_count());
    EXPECT_EQ(std::string(""), GetMonsterCookies(monster));
    EXPECT_EQ(GetMonsterCookies(monster),
              GetDisplayedCookies(cookies_model.get()));
    EXPECT_TRUE(mock_browsing_data_database_helper_->AllDeleted());
    EXPECT_TRUE(mock_browsing_data_local_storage_helper_->AllDeleted());
    EXPECT_FALSE(mock_browsing_data_session_storage_helper_->AllDeleted());
    EXPECT_TRUE(mock_browsing_data_indexed_db_helper_->AllDeleted());
  }
}

TEST_F(CookiesTreeModelTest, Remove) {
  scoped_ptr<CookiesTreeModel> cookies_model(
      CreateCookiesTreeModelWithInitialSample());
  net::CookieMonster* monster = profile_->GetCookieMonster();

  DeleteStoredObjects(cookies_model->GetRoot()->GetChild(0));
  {
    SCOPED_TRACE("First cookie origin removed");
    EXPECT_STREQ("B,C", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("db1,db2", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ(29, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  DeleteStoredObjects(cookies_model->GetRoot()->GetChild(2));
  {
    SCOPED_TRACE("First database origin removed");
    EXPECT_STREQ("B,C", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("db2", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ(26, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  DeleteStoredObjects(cookies_model->GetRoot()->GetChild(3));
  {
    SCOPED_TRACE("First local storage origin removed");
    EXPECT_STREQ("B,C", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("db2", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ(21, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  DeleteStoredObjects(cookies_model->GetRoot()->GetChild(4));
  {
    SCOPED_TRACE("First IndexedDB origin removed");
    EXPECT_STREQ("B,C", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("db2", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ(18, cookies_model->GetRoot()->GetTotalNodeCount());
  }
}

TEST_F(CookiesTreeModelTest, RemoveCookiesNode) {
  scoped_ptr<CookiesTreeModel> cookies_model(
      CreateCookiesTreeModelWithInitialSample());
  net::CookieMonster* monster = profile_->GetCookieMonster();

  DeleteStoredObjects(cookies_model->GetRoot()->GetChild(0)->GetChild(0));
  {
    SCOPED_TRACE("First origin removed");
    EXPECT_STREQ("B,C", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    // 28 because in this case, the origin remains, although the COOKIES
    // node beneath it has been deleted. So, we have
    // root -> foo1 -> cookies -> a, foo2, foo3 -> cookies -> c
    // dbhost1 -> database -> db1, dbhost2 -> database -> db2,
    // host1 -> localstorage -> http://host1:1/,
    // host2 -> localstorage -> http://host2:2/,
    // idbhost1 -> sessionstorage -> http://idbhost1:1/,
    // idbhost2 -> sessionstorage -> http://idbhost2:2/.
    EXPECT_EQ(30, cookies_model->GetRoot()->GetTotalNodeCount());
    EXPECT_EQ("db1,db2", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
  }

  DeleteStoredObjects(cookies_model->GetRoot()->GetChild(3)->GetChild(0));
  {
    SCOPED_TRACE("First database removed");
    EXPECT_STREQ("B,C", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("db2", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ(28, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  DeleteStoredObjects(cookies_model->GetRoot()->GetChild(5)->GetChild(0));
  {
    SCOPED_TRACE("First origin removed");
    EXPECT_STREQ("B,C", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("db2", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ(26, cookies_model->GetRoot()->GetTotalNodeCount());
  }
}

TEST_F(CookiesTreeModelTest, RemoveCookieNode) {
  scoped_ptr<CookiesTreeModel> cookies_model(
      CreateCookiesTreeModelWithInitialSample());
  net::CookieMonster* monster = profile_->GetCookieMonster();

  DeleteStoredObjects(cookies_model->GetRoot()->GetChild(1)->GetChild(0));
  {
    SCOPED_TRACE("Second origin COOKIES node removed");
    EXPECT_STREQ("A,C", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("A,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("db1,db2", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    // 28 because in this case, the origin remains, although the COOKIES
    // node beneath it has been deleted. So, we have
    // root -> foo1 -> cookies -> a, foo2, foo3 -> cookies -> c
    // dbhost1 -> database -> db1, dbhost2 -> database -> db2,
    // host1 -> localstorage -> http://host1:1/,
    // host2 -> localstorage -> http://host2:2/.
    // host1 -> sessionstorage -> http://host1:1/,
    // host2 -> sessionstorage -> http://host2:2/,
    // idbhost1 -> sessionstorage -> http://idbhost1:1/,
    // idbhost2 -> sessionstorage -> http://idbhost2:2/.
    EXPECT_EQ(30, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  DeleteStoredObjects(cookies_model->GetRoot()->GetChild(3)->GetChild(0));
  {
    SCOPED_TRACE("First database removed");
    EXPECT_STREQ("A,C", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("A,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("db2", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ(28, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  DeleteStoredObjects(cookies_model->GetRoot()->GetChild(5)->GetChild(0));
  {
    SCOPED_TRACE("First origin removed");
    EXPECT_STREQ("A,C", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("A,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("db2", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ(26, cookies_model->GetRoot()->GetTotalNodeCount());
  }
}

TEST_F(CookiesTreeModelTest, RemoveSingleCookieNode) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo1"), "A=1");
  monster->SetCookie(GURL("http://foo2"), "B=1");
  monster->SetCookie(GURL("http://foo3"), "C=1");
  monster->SetCookie(GURL("http://foo3"), "D=1");
  CookiesTreeModel cookies_model(monster,
                                 mock_browsing_data_database_helper_,
                                 mock_browsing_data_local_storage_helper_,
                                 mock_browsing_data_session_storage_helper_,
                                 mock_browsing_data_appcache_helper_,
                                 mock_browsing_data_indexed_db_helper_);
  mock_browsing_data_database_helper_->AddDatabaseSamples();
  mock_browsing_data_database_helper_->Notify();
  mock_browsing_data_local_storage_helper_->AddLocalStorageSamples();
  mock_browsing_data_local_storage_helper_->Notify();
  mock_browsing_data_session_storage_helper_->AddLocalStorageSamples();
  mock_browsing_data_session_storage_helper_->Notify();
  mock_browsing_data_indexed_db_helper_->AddIndexedDBSamples();
  mock_browsing_data_indexed_db_helper_->Notify();

  {
    SCOPED_TRACE("Initial State 4 cookies, 2 databases, 2 local storages, "
        "2 session storages, 2 indexed DBs");
    // 33 because there's the root, then foo1 -> cookies -> a,
    // foo2 -> cookies -> b, foo3 -> cookies -> c,d
    // dbhost1 -> database -> db1, dbhost2 -> database -> db2,
    // host1 -> localstorage -> http://host1:1/,
    // host2 -> localstorage -> http://host2:2/.
    // host1 -> sessionstorage -> http://host1:1/,
    // host2 -> sessionstorage -> http://host2:2/,
    // idbhost1 -> sessionstorage -> http://idbhost1:1/,
    // idbhost2 -> sessionstorage -> http://idbhost2:2/.
    EXPECT_EQ(33, cookies_model.GetRoot()->GetTotalNodeCount());
    EXPECT_STREQ("A,B,C,D", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("A,B,C,D", GetDisplayedCookies(&cookies_model).c_str());
    EXPECT_EQ("db1,db2", GetDisplayedDatabases(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(&cookies_model));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(&cookies_model));
  }
  DeleteStoredObjects(cookies_model.GetRoot()->GetChild(2));
  {
    SCOPED_TRACE("Third origin removed");
    EXPECT_STREQ("A,B", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("A,B", GetDisplayedCookies(&cookies_model).c_str());
    EXPECT_EQ("db1,db2", GetDisplayedDatabases(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(&cookies_model));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(&cookies_model));
    EXPECT_EQ(29, cookies_model.GetRoot()->GetTotalNodeCount());
  }
}

TEST_F(CookiesTreeModelTest, RemoveSingleCookieNodeOf3) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo1"), "A=1");
  monster->SetCookie(GURL("http://foo2"), "B=1");
  monster->SetCookie(GURL("http://foo3"), "C=1");
  monster->SetCookie(GURL("http://foo3"), "D=1");
  monster->SetCookie(GURL("http://foo3"), "E=1");
  CookiesTreeModel cookies_model(monster,
                                 mock_browsing_data_database_helper_,
                                 mock_browsing_data_local_storage_helper_,
                                 mock_browsing_data_session_storage_helper_,
                                 mock_browsing_data_appcache_helper_,
                                 mock_browsing_data_indexed_db_helper_);
  mock_browsing_data_database_helper_->AddDatabaseSamples();
  mock_browsing_data_database_helper_->Notify();
  mock_browsing_data_local_storage_helper_->AddLocalStorageSamples();
  mock_browsing_data_local_storage_helper_->Notify();
  mock_browsing_data_session_storage_helper_->AddLocalStorageSamples();
  mock_browsing_data_session_storage_helper_->Notify();
  mock_browsing_data_indexed_db_helper_->AddIndexedDBSamples();
  mock_browsing_data_indexed_db_helper_->Notify();

  {
    SCOPED_TRACE("Initial State 5 cookies, 2 databases, 2 local storages, "
                 "2 session storages, 2 indexed DBs");
    // 34 because there's the root, then foo1 -> cookies -> a,
    // foo2 -> cookies -> b, foo3 -> cookies -> c,d,e
    // dbhost1 -> database -> db1, dbhost2 -> database -> db2,
    // host1 -> localstorage -> http://host1:1/,
    // host2 -> localstorage -> http://host2:2/.
    // host1 -> sessionstorage -> http://host1:1/,
    // host2 -> sessionstorage -> http://host2:2/,
    // idbhost1 -> sessionstorage -> http://idbhost1:1/,
    // idbhost2 -> sessionstorage -> http://idbhost2:2/.
    EXPECT_EQ(34, cookies_model.GetRoot()->GetTotalNodeCount());
    EXPECT_STREQ("A,B,C,D,E", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("A,B,C,D,E", GetDisplayedCookies(&cookies_model).c_str());
    EXPECT_EQ("db1,db2", GetDisplayedDatabases(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(&cookies_model));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(&cookies_model));
  }
  DeleteStoredObjects(cookies_model.GetRoot()->GetChild(2)->GetChild(0)->
      GetChild(1));
  {
    SCOPED_TRACE("Middle cookie in third origin removed");
    EXPECT_STREQ("A,B,C,E", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("A,B,C,E", GetDisplayedCookies(&cookies_model).c_str());
    EXPECT_EQ(33, cookies_model.GetRoot()->GetTotalNodeCount());
    EXPECT_EQ("db1,db2", GetDisplayedDatabases(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(&cookies_model));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(&cookies_model));
  }
}

TEST_F(CookiesTreeModelTest, RemoveSecondOrigin) {
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(GURL("http://foo1"), "A=1");
  monster->SetCookie(GURL("http://foo2"), "B=1");
  monster->SetCookie(GURL("http://foo3"), "C=1");
  monster->SetCookie(GURL("http://foo3"), "D=1");
  monster->SetCookie(GURL("http://foo3"), "E=1");
  CookiesTreeModel cookies_model(monster,
                                 mock_browsing_data_database_helper_,
                                 mock_browsing_data_local_storage_helper_,
                                 mock_browsing_data_session_storage_helper_,
                                 mock_browsing_data_appcache_helper_,
                                 mock_browsing_data_indexed_db_helper_);
  {
    SCOPED_TRACE("Initial State 5 cookies");
    // 11 because there's the root, then foo1 -> cookies -> a,
    // foo2 -> cookies -> b, foo3 -> cookies -> c,d,e
    EXPECT_EQ(12, cookies_model.GetRoot()->GetTotalNodeCount());
    EXPECT_STREQ("A,B,C,D,E", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("A,B,C,D,E", GetDisplayedCookies(&cookies_model).c_str());
  }
  DeleteStoredObjects(cookies_model.GetRoot()->GetChild(1));
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

  CookiesTreeModel cookies_model(monster,
      new MockBrowsingDataDatabaseHelper(profile_.get()),
      new MockBrowsingDataLocalStorageHelper(profile_.get()),
      new MockBrowsingDataLocalStorageHelper(profile_.get()),
      new MockBrowsingDataAppCacheHelper(profile_.get()),
      new MockBrowsingDataIndexedDBHelper(profile_.get()));

  {
    SCOPED_TRACE("Initial State 8 cookies");
    // CookieMonster orders cookies by pathlength, then by creation time.
    // All paths are length 1.
    EXPECT_STREQ("A,B,C,D,E,F,G,H", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("F,E,C,B,A,G,D,H",
        GetDisplayedCookies(&cookies_model).c_str());
  }
  DeleteStoredObjects(cookies_model.GetRoot()->GetChild(1));  // Delete "E"
  {
    EXPECT_STREQ("A,B,C,D,F,G,H", GetMonsterCookies(monster).c_str());
    EXPECT_STREQ("F,C,B,A,G,D,H", GetDisplayedCookies(&cookies_model).c_str());
  }
}

TEST_F(CookiesTreeModelTest, ContentSettings) {
  GURL host("http://example.com/");
  ContentSettingsPattern pattern("[*.]example.com");
  net::CookieMonster* monster = profile_->GetCookieMonster();
  monster->SetCookie(host, "A=1");

  CookiesTreeModel cookies_model(monster,
      new MockBrowsingDataDatabaseHelper(profile_.get()),
      new MockBrowsingDataLocalStorageHelper(profile_.get()),
      new MockBrowsingDataLocalStorageHelper(profile_.get()),
      new MockBrowsingDataAppCacheHelper(profile_.get()),
      new MockBrowsingDataIndexedDBHelper(profile_.get()));

  TestingProfile profile;
  HostContentSettingsMap* content_settings =
      profile.GetHostContentSettingsMap();
  StubSettingsObserver observer;

  CookieTreeRootNode* root =
      static_cast<CookieTreeRootNode*>(cookies_model.GetRoot());
  CookieTreeOriginNode* origin = root->GetOrCreateOriginNode(host);

  EXPECT_EQ(1, origin->child_count());
  EXPECT_TRUE(origin->CanCreateContentException());
  origin->CreateContentException(
      content_settings, CONTENT_SETTING_SESSION_ONLY);

  EXPECT_EQ(2, observer.counter);
  EXPECT_EQ(pattern, observer.last_pattern);
  EXPECT_EQ(CONTENT_SETTING_SESSION_ONLY,
      content_settings->GetContentSetting(
          host, CONTENT_SETTINGS_TYPE_COOKIES, ""));
}

}  // namespace
