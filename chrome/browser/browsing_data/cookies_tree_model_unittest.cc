// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/cookies_tree_model.h"

#include <string>

#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browsing_data/mock_browsing_data_appcache_helper.h"
#include "chrome/browser/browsing_data/mock_browsing_data_channel_id_helper.h"
#include "chrome/browser/browsing_data/mock_browsing_data_cookie_helper.h"
#include "chrome/browser/browsing_data/mock_browsing_data_database_helper.h"
#include "chrome/browser/browsing_data/mock_browsing_data_file_system_helper.h"
#include "chrome/browser/browsing_data/mock_browsing_data_flash_lso_helper.h"
#include "chrome/browser/browsing_data/mock_browsing_data_indexed_db_helper.h"
#include "chrome/browser/browsing_data/mock_browsing_data_local_storage_helper.h"
#include "chrome/browser/browsing_data/mock_browsing_data_quota_helper.h"
#include "chrome/browser/browsing_data/mock_browsing_data_service_worker_helper.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/mock_settings_observer.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_types.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#endif

using ::testing::_;
using content::BrowserThread;

namespace {

class CookiesTreeModelTest : public testing::Test {
 public:
  virtual ~CookiesTreeModelTest() {
    // Avoid memory leaks.
#if defined(ENABLE_EXTENSIONS)
    special_storage_policy_ = NULL;
#endif
    profile_.reset();
    base::MessageLoop::current()->RunUntilIdle();
  }

  virtual void SetUp() OVERRIDE {
    profile_.reset(new TestingProfile());
    mock_browsing_data_cookie_helper_ =
        new MockBrowsingDataCookieHelper(profile_->GetRequestContext());
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
    mock_browsing_data_file_system_helper_ =
        new MockBrowsingDataFileSystemHelper(profile_.get());
    mock_browsing_data_quota_helper_ =
        new MockBrowsingDataQuotaHelper(profile_.get());
    mock_browsing_data_channel_id_helper_ =
        new MockBrowsingDataChannelIDHelper();
    mock_browsing_data_service_worker_helper_ =
        new MockBrowsingDataServiceWorkerHelper(profile_.get());
    mock_browsing_data_flash_lso_helper_ =
        new MockBrowsingDataFlashLSOHelper(profile_.get());

    scoped_refptr<CookieSettings> cookie_settings =
        new CookieSettings(profile_->GetHostContentSettingsMap(),
                           profile_->GetPrefs());
#if defined(ENABLE_EXTENSIONS)
    special_storage_policy_ =
        new ExtensionSpecialStoragePolicy(cookie_settings.get());
#endif
  }

  virtual void TearDown() OVERRIDE {
    mock_browsing_data_service_worker_helper_ = NULL;
    mock_browsing_data_channel_id_helper_ = NULL;
    mock_browsing_data_quota_helper_ = NULL;
    mock_browsing_data_file_system_helper_ = NULL;
    mock_browsing_data_indexed_db_helper_ = NULL;
    mock_browsing_data_appcache_helper_ = NULL;
    mock_browsing_data_session_storage_helper_ = NULL;
    mock_browsing_data_local_storage_helper_ = NULL;
    mock_browsing_data_database_helper_ = NULL;
    mock_browsing_data_flash_lso_helper_ = NULL;
    base::MessageLoop::current()->RunUntilIdle();
  }

  scoped_ptr<CookiesTreeModel> CreateCookiesTreeModelWithInitialSample() {
    LocalDataContainer* container = new LocalDataContainer(
        mock_browsing_data_cookie_helper_.get(),
        mock_browsing_data_database_helper_.get(),
        mock_browsing_data_local_storage_helper_.get(),
        mock_browsing_data_session_storage_helper_.get(),
        mock_browsing_data_appcache_helper_.get(),
        mock_browsing_data_indexed_db_helper_.get(),
        mock_browsing_data_file_system_helper_.get(),
        mock_browsing_data_quota_helper_.get(),
        mock_browsing_data_channel_id_helper_.get(),
        mock_browsing_data_service_worker_helper_.get(),
        mock_browsing_data_flash_lso_helper_.get());

    CookiesTreeModel* cookies_model =
        new CookiesTreeModel(container, special_storage_policy(), false);
    mock_browsing_data_cookie_helper_->
        AddCookieSamples(GURL("http://foo1"), "A=1");
    mock_browsing_data_cookie_helper_->
        AddCookieSamples(GURL("http://foo2"), "B=1");
    mock_browsing_data_cookie_helper_->
        AddCookieSamples(GURL("http://foo3"), "C=1");
    mock_browsing_data_cookie_helper_->Notify();
    mock_browsing_data_database_helper_->AddDatabaseSamples();
    mock_browsing_data_database_helper_->Notify();
    mock_browsing_data_local_storage_helper_->AddLocalStorageSamples();
    mock_browsing_data_local_storage_helper_->Notify();
    mock_browsing_data_session_storage_helper_->AddLocalStorageSamples();
    mock_browsing_data_session_storage_helper_->Notify();
    mock_browsing_data_indexed_db_helper_->AddIndexedDBSamples();
    mock_browsing_data_indexed_db_helper_->Notify();
    mock_browsing_data_file_system_helper_->AddFileSystemSamples();
    mock_browsing_data_file_system_helper_->Notify();
    mock_browsing_data_quota_helper_->AddQuotaSamples();
    mock_browsing_data_quota_helper_->Notify();
    mock_browsing_data_channel_id_helper_->AddChannelIDSample(
        "sbc1");
    mock_browsing_data_channel_id_helper_->AddChannelIDSample(
        "sbc2");
    mock_browsing_data_channel_id_helper_->Notify();
    mock_browsing_data_service_worker_helper_->AddServiceWorkerSamples();
    mock_browsing_data_service_worker_helper_->Notify();
    mock_browsing_data_flash_lso_helper_->AddFlashLSODomain("xyz.com");
    mock_browsing_data_flash_lso_helper_->Notify();

    {
      SCOPED_TRACE("Initial State 3 cookies, 2 databases, 2 local storages, "
                   "2 session storages, 2 indexed DBs, 3 filesystems, "
                   "2 quotas, 2 server bound certs, 2 service workers, "
                   "1 Flash LSO");
      // 59 because there's the root, then
      // foo1 -> cookies -> a,
      // foo2 -> cookies -> b,
      // foo3 -> cookies -> c,
      // dbhost1 -> database -> db1,
      // dbhost2 -> database -> db2,
      // host1 -> localstorage -> http://host1:1/,
      //       -> sessionstorage -> http://host1:1/,
      // host2 -> localstorage -> http://host2:2/.
      //       -> sessionstorage -> http://host2:2/,
      // idbhost1 -> indexeddb -> http://idbhost1:1/,
      // idbhost2 -> indexeddb -> http://idbhost2:2/,
      // fshost1 -> filesystem -> http://fshost1:1/,
      // fshost2 -> filesystem -> http://fshost2:1/,
      // fshost3 -> filesystem -> http://fshost3:1/,
      // quotahost1 -> quotahost1,
      // quotahost2 -> quotahost2,
      // sbc1 -> sbcerts -> sbc1,
      // sbc2 -> sbcerts -> sbc2.
      // swhost1 -> service worker -> https://swhost1:1
      // swhost2 -> service worker -> https://swhost1:2
      // xyz.com -> flash_lsos
      EXPECT_EQ(59, cookies_model->GetRoot()->GetTotalNodeCount());
      EXPECT_EQ("A,B,C", GetDisplayedCookies(cookies_model));
      EXPECT_EQ("db1,db2", GetDisplayedDatabases(cookies_model));
      EXPECT_EQ("http://host1:1/,http://host2:2/",
                GetDisplayedLocalStorages(cookies_model));
      EXPECT_EQ("http://host1:1/,http://host2:2/",
                GetDisplayedSessionStorages(cookies_model));
      EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
                GetDisplayedIndexedDBs(cookies_model));
      EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
                GetDisplayedFileSystems(cookies_model));
      EXPECT_EQ("quotahost1,quotahost2",
                GetDisplayedQuotas(cookies_model));
      EXPECT_EQ("sbc1,sbc2",
                GetDisplayedChannelIDs(cookies_model));
      EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
                GetDisplayedServiceWorkers(cookies_model));
      EXPECT_EQ("xyz.com",
                GetDisplayedFlashLSOs(cookies_model));
    }
    return make_scoped_ptr(cookies_model);
  }

  std::string GetNodesOfChildren(
      const CookieTreeNode* node,
      CookieTreeNode::DetailedInfo::NodeType node_type) {
    if (!node->empty()) {
      std::string retval;
      for (int i = 0; i < node->child_count(); ++i) {
        retval += GetNodesOfChildren(node->GetChild(i), node_type);
      }
      return retval;
    }

    if (node->GetDetailedInfo().node_type != node_type)
      return std::string();

    switch (node_type) {
      case CookieTreeNode::DetailedInfo::TYPE_SESSION_STORAGE:
        return node->GetDetailedInfo().session_storage_info->origin_url.spec() +
            ",";
      case CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGE:
        return node->GetDetailedInfo().local_storage_info->origin_url.spec() +
            ",";
      case CookieTreeNode::DetailedInfo::TYPE_DATABASE:
        return node->GetDetailedInfo().database_info->database_name + ",";
      case CookieTreeNode::DetailedInfo::TYPE_COOKIE:
        return node->GetDetailedInfo().cookie->Name() + ",";
      case CookieTreeNode::DetailedInfo::TYPE_APPCACHE:
        return node->GetDetailedInfo().appcache_info->manifest_url.spec() +
            ",";
      case CookieTreeNode::DetailedInfo::TYPE_INDEXED_DB:
        return node->GetDetailedInfo().indexed_db_info->origin_.spec() +
            ",";
      case CookieTreeNode::DetailedInfo::TYPE_FILE_SYSTEM:
        return node->GetDetailedInfo().file_system_info->origin.spec() +
            ",";
      case CookieTreeNode::DetailedInfo::TYPE_QUOTA:
        return node->GetDetailedInfo().quota_info->host + ",";
      case CookieTreeNode::DetailedInfo::TYPE_CHANNEL_ID:
        return node->GetDetailedInfo().channel_id->server_identifier() + ",";
      case CookieTreeNode::DetailedInfo::TYPE_SERVICE_WORKER:
        return node->GetDetailedInfo().service_worker_info->origin.spec() + ",";
      case CookieTreeNode::DetailedInfo::TYPE_FLASH_LSO:
        return node->GetDetailedInfo().flash_lso_domain + ",";
      default:
        return std::string();
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

  std::string GetFileSystemsOfChildren(const CookieTreeNode* node) {
    return GetNodesOfChildren(
        node, CookieTreeNode::DetailedInfo::TYPE_FILE_SYSTEM);
  }

  std::string GetFileQuotaOfChildren(const CookieTreeNode* node) {
    return GetNodesOfChildren(
        node, CookieTreeNode::DetailedInfo::TYPE_QUOTA);
  }

  std::string GetServiceWorkersOfChildren(const CookieTreeNode* node) {
    return GetNodesOfChildren(
        node, CookieTreeNode::DetailedInfo::TYPE_SERVICE_WORKER);
  }

  std::string GetFlashLSOsOfChildren(const CookieTreeNode* node) {
    return GetNodesOfChildren(
        node, CookieTreeNode::DetailedInfo::TYPE_FLASH_LSO);
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

  std::string GetDisplayedFileSystems(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(cookies_model,
                             CookieTreeNode::DetailedInfo::TYPE_FILE_SYSTEM);
  }

  std::string GetDisplayedQuotas(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(cookies_model,
                             CookieTreeNode::DetailedInfo::TYPE_QUOTA);
  }

  std::string GetDisplayedChannelIDs(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(
        cookies_model, CookieTreeNode::DetailedInfo::TYPE_CHANNEL_ID);
  }

  std::string GetDisplayedServiceWorkers(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(cookies_model,
                             CookieTreeNode::DetailedInfo::TYPE_SERVICE_WORKER);
  }

  std::string GetDisplayedFlashLSOs(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(
        cookies_model, CookieTreeNode::DetailedInfo::TYPE_FLASH_LSO);
  }

  // Do not call on the root.
  void DeleteStoredObjects(CookieTreeNode* node) {
    node->DeleteStoredObjects();
    CookieTreeNode* parent_node = node->parent();
    DCHECK(parent_node);
    delete parent_node->GetModel()->Remove(parent_node, node);
  }

 protected:
  ExtensionSpecialStoragePolicy* special_storage_policy() {
#if defined(ENABLE_EXTENSIONS)
    return special_storage_policy_.get();
#else
    return NULL;
#endif
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<TestingProfile> profile_;
  scoped_refptr<MockBrowsingDataCookieHelper>
      mock_browsing_data_cookie_helper_;
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
  scoped_refptr<MockBrowsingDataFileSystemHelper>
      mock_browsing_data_file_system_helper_;
  scoped_refptr<MockBrowsingDataQuotaHelper>
      mock_browsing_data_quota_helper_;
  scoped_refptr<MockBrowsingDataChannelIDHelper>
      mock_browsing_data_channel_id_helper_;
  scoped_refptr<MockBrowsingDataServiceWorkerHelper>
      mock_browsing_data_service_worker_helper_;
  scoped_refptr<MockBrowsingDataFlashLSOHelper>
      mock_browsing_data_flash_lso_helper_;

#if defined(ENABLE_EXTENSIONS)
  scoped_refptr<ExtensionSpecialStoragePolicy> special_storage_policy_;
#endif
};

TEST_F(CookiesTreeModelTest, RemoveAll) {
  scoped_ptr<CookiesTreeModel> cookies_model(
      CreateCookiesTreeModelWithInitialSample());

  // Reset the selection of the first row.
  {
    SCOPED_TRACE("Before removing");
    EXPECT_EQ("A,B,C",
              GetDisplayedCookies(cookies_model.get()));
    EXPECT_EQ("db1,db2",
              GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2",
              GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("sbc1,sbc2",
              GetDisplayedChannelIDs(cookies_model.get()));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("xyz.com",
              GetDisplayedFlashLSOs(cookies_model.get()));
  }

  mock_browsing_data_cookie_helper_->Reset();
  mock_browsing_data_database_helper_->Reset();
  mock_browsing_data_local_storage_helper_->Reset();
  mock_browsing_data_session_storage_helper_->Reset();
  mock_browsing_data_indexed_db_helper_->Reset();
  mock_browsing_data_service_worker_helper_->Reset();
  mock_browsing_data_file_system_helper_->Reset();

  cookies_model->DeleteAllStoredObjects();

  // Make sure the nodes are also deleted from the model's cache.
  // http://crbug.com/43249
  cookies_model->UpdateSearchResults(base::string16());

  {
    // 2 nodes - root and app
    SCOPED_TRACE("After removing");
    EXPECT_EQ(1, cookies_model->GetRoot()->GetTotalNodeCount());
    EXPECT_EQ(0, cookies_model->GetRoot()->child_count());
    EXPECT_EQ(std::string(), GetDisplayedCookies(cookies_model.get()));
    EXPECT_TRUE(mock_browsing_data_cookie_helper_->AllDeleted());
    EXPECT_TRUE(mock_browsing_data_database_helper_->AllDeleted());
    EXPECT_TRUE(mock_browsing_data_local_storage_helper_->AllDeleted());
    EXPECT_FALSE(mock_browsing_data_session_storage_helper_->AllDeleted());
    EXPECT_TRUE(mock_browsing_data_indexed_db_helper_->AllDeleted());
    EXPECT_TRUE(mock_browsing_data_file_system_helper_->AllDeleted());
    EXPECT_TRUE(mock_browsing_data_channel_id_helper_->AllDeleted());
    EXPECT_TRUE(mock_browsing_data_service_worker_helper_->AllDeleted());
    EXPECT_TRUE(mock_browsing_data_flash_lso_helper_->AllDeleted());
  }
}

TEST_F(CookiesTreeModelTest, Remove) {
  scoped_ptr<CookiesTreeModel> cookies_model(
      CreateCookiesTreeModelWithInitialSample());

  // Children start out arranged as follows:
  //
  // 0. `foo1`
  // 1. `foo2`
  // 2. `foo3`
  // 3. `fshost1`
  // 4. `fshost2`
  // 5. `fshost3`
  // 6. `gdbhost1`
  // 7. `gdbhost2`
  // 8. `host1`
  // 9. `host2`
  // 10. `idbhost1`
  // 11. `idbhost2`
  // 12. `quotahost1`
  // 13. `quotahost2`
  // 14. `sbc1`
  // 15. `sbc2`
  // 16. 'swhost1'
  // 17. 'swhost2'
  // 18. `xyz.com`
  //
  // Here, we'll remove them one by one, starting from the end, and
  // check that the state makes sense.

  DeleteStoredObjects(cookies_model->GetRoot()->GetChild(18));
  {
    SCOPED_TRACE("`xyz.com` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("db1,db2", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2",
              GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("sbc1,sbc2",
              GetDisplayedChannelIDs(cookies_model.get()));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(57, cookies_model->GetRoot()->GetTotalNodeCount());
  }
  DeleteStoredObjects(cookies_model->GetRoot()->GetChild(17));
  {
    SCOPED_TRACE("`swhost2` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("db1,db2", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2",
              GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("sbc1,sbc2",
              GetDisplayedChannelIDs(cookies_model.get()));
    EXPECT_EQ("https://swhost1:1/",
              GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(54, cookies_model->GetRoot()->GetTotalNodeCount());
  }
  DeleteStoredObjects(cookies_model->GetRoot()->GetChild(16));
  {
    SCOPED_TRACE("`swhost1` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("db1,db2", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2",
              GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("sbc1,sbc2",
              GetDisplayedChannelIDs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(51, cookies_model->GetRoot()->GetTotalNodeCount());
  }
  DeleteStoredObjects(cookies_model->GetRoot()->GetChild(15));
  {
    SCOPED_TRACE("`sbc2` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("db1,db2", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2",
              GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("sbc1",
              GetDisplayedChannelIDs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(48, cookies_model->GetRoot()->GetTotalNodeCount());
  }
  DeleteStoredObjects(cookies_model->GetRoot()->GetChild(14));
  {
    SCOPED_TRACE("`sbc1` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("db1,db2", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2",
              GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(45, cookies_model->GetRoot()->GetTotalNodeCount());
  }
  DeleteStoredObjects(cookies_model->GetRoot()->GetChild(13));
  {
    SCOPED_TRACE("`quotahost2` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("db1,db2", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("quotahost1",
              GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(43, cookies_model->GetRoot()->GetTotalNodeCount());
  }
  DeleteStoredObjects(cookies_model->GetRoot()->GetChild(12));
  {
    SCOPED_TRACE("`quotahost1` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("db1,db2", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(41, cookies_model->GetRoot()->GetTotalNodeCount());
  }
  DeleteStoredObjects(cookies_model->GetRoot()->GetChild(11));
  {
    SCOPED_TRACE("`idbhost2` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("db1,db2", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(38, cookies_model->GetRoot()->GetTotalNodeCount());
  }
  DeleteStoredObjects(cookies_model->GetRoot()->GetChild(10));
  {
    SCOPED_TRACE("`idbhost1` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("db1,db2", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(35, cookies_model->GetRoot()->GetTotalNodeCount());
  }
  DeleteStoredObjects(cookies_model->GetRoot()->GetChild(9));
  {
    SCOPED_TRACE("`host2` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("db1,db2", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(30, cookies_model->GetRoot()->GetTotalNodeCount());
  }
  DeleteStoredObjects(cookies_model->GetRoot()->GetChild(8));
  {
    SCOPED_TRACE("`host1` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("db1,db2", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(25, cookies_model->GetRoot()->GetTotalNodeCount());
  }
  DeleteStoredObjects(cookies_model->GetRoot()->GetChild(7));
  {
    SCOPED_TRACE("`gdbhost2` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("db1", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(22, cookies_model->GetRoot()->GetTotalNodeCount());
  }
  DeleteStoredObjects(cookies_model->GetRoot()->GetChild(6));
  {
    SCOPED_TRACE("`gdbhost1` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(19, cookies_model->GetRoot()->GetTotalNodeCount());
  }
  DeleteStoredObjects(cookies_model->GetRoot()->GetChild(5));
  {
    SCOPED_TRACE("`fshost3` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(16, cookies_model->GetRoot()->GetTotalNodeCount());
  }
  DeleteStoredObjects(cookies_model->GetRoot()->GetChild(4));
  {
    SCOPED_TRACE("`fshost2` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(13, cookies_model->GetRoot()->GetTotalNodeCount());
  }
  DeleteStoredObjects(cookies_model->GetRoot()->GetChild(3));
  {
    SCOPED_TRACE("`fshost1` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(10, cookies_model->GetRoot()->GetTotalNodeCount());
  }
  DeleteStoredObjects(cookies_model->GetRoot()->GetChild(2));
  {
    SCOPED_TRACE("`foo3` removed.");
    EXPECT_STREQ("A,B", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(7, cookies_model->GetRoot()->GetTotalNodeCount());
  }
  DeleteStoredObjects(cookies_model->GetRoot()->GetChild(1));
  {
    SCOPED_TRACE("`foo2` removed.");
    EXPECT_STREQ("A", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(4, cookies_model->GetRoot()->GetTotalNodeCount());
  }
  DeleteStoredObjects(cookies_model->GetRoot()->GetChild(0));
  {
    SCOPED_TRACE("`foo1` removed.");
    EXPECT_STREQ("", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(1, cookies_model->GetRoot()->GetTotalNodeCount());
  }
}

TEST_F(CookiesTreeModelTest, RemoveCookiesNode) {
  scoped_ptr<CookiesTreeModel> cookies_model(
      CreateCookiesTreeModelWithInitialSample());

  DeleteStoredObjects(
      cookies_model->GetRoot()->GetChild(0)->GetChild(0));
  {
    SCOPED_TRACE("First origin removed");
    EXPECT_STREQ("B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    // 57 because in this case, the origin remains, although the COOKIES
    // node beneath it has been deleted.
    EXPECT_EQ(57, cookies_model->GetRoot()->GetTotalNodeCount());
    EXPECT_EQ("db1,db2", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("sbc1,sbc2", GetDisplayedChannelIDs(cookies_model.get()));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(cookies_model.get()));
  }

  DeleteStoredObjects(
      cookies_model->GetRoot()->GetChild(6)->GetChild(0));
  {
    SCOPED_TRACE("First database removed");
    EXPECT_STREQ("B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("db2", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("sbc1,sbc2", GetDisplayedChannelIDs(cookies_model.get()));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(55, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  DeleteStoredObjects(
      cookies_model->GetRoot()->GetChild(8)->GetChild(0));
  {
    SCOPED_TRACE("First origin removed");
    EXPECT_STREQ("B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("db2", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("sbc1,sbc2", GetDisplayedChannelIDs(cookies_model.get()));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(53, cookies_model->GetRoot()->GetTotalNodeCount());
  }
}

TEST_F(CookiesTreeModelTest, RemoveCookieNode) {
  scoped_ptr<CookiesTreeModel> cookies_model(
      CreateCookiesTreeModelWithInitialSample());

  DeleteStoredObjects(
      cookies_model->GetRoot()->GetChild(1)->GetChild(0));
  {
    SCOPED_TRACE("Second origin COOKIES node removed");
    EXPECT_STREQ("A,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("db1,db2", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("sbc1,sbc2", GetDisplayedChannelIDs(cookies_model.get()));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(cookies_model.get()));
    // 57 because in this case, the origin remains, although the COOKIES
    // node beneath it has been deleted.
    EXPECT_EQ(57, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  DeleteStoredObjects(
      cookies_model->GetRoot()->GetChild(6)->GetChild(0));
  {
    SCOPED_TRACE("First database removed");
    EXPECT_STREQ("A,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("db2", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("sbc1,sbc2", GetDisplayedChannelIDs(cookies_model.get()));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(55, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  DeleteStoredObjects(
      cookies_model->GetRoot()->GetChild(8)->GetChild(0));
  {
    SCOPED_TRACE("First origin removed");
    EXPECT_STREQ("A,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("db2", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("sbc1,sbc2", GetDisplayedChannelIDs(cookies_model.get()));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(53, cookies_model->GetRoot()->GetTotalNodeCount());
  }
}

TEST_F(CookiesTreeModelTest, RemoveSingleCookieNode) {
  LocalDataContainer* container =
      new LocalDataContainer(mock_browsing_data_cookie_helper_.get(),
                             mock_browsing_data_database_helper_.get(),
                             mock_browsing_data_local_storage_helper_.get(),
                             mock_browsing_data_session_storage_helper_.get(),
                             mock_browsing_data_appcache_helper_.get(),
                             mock_browsing_data_indexed_db_helper_.get(),
                             mock_browsing_data_file_system_helper_.get(),
                             mock_browsing_data_quota_helper_.get(),
                             mock_browsing_data_channel_id_helper_.get(),
                             mock_browsing_data_service_worker_helper_.get(),
                             mock_browsing_data_flash_lso_helper_.get());
  CookiesTreeModel cookies_model(container, special_storage_policy(), false);

  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo1"), "A=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo2"), "B=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3"), "C=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3"), "D=1");
  mock_browsing_data_cookie_helper_->Notify();
  mock_browsing_data_database_helper_->AddDatabaseSamples();
  mock_browsing_data_database_helper_->Notify();
  mock_browsing_data_local_storage_helper_->AddLocalStorageSamples();
  mock_browsing_data_local_storage_helper_->Notify();
  mock_browsing_data_session_storage_helper_->AddLocalStorageSamples();
  mock_browsing_data_session_storage_helper_->Notify();
  mock_browsing_data_indexed_db_helper_->AddIndexedDBSamples();
  mock_browsing_data_indexed_db_helper_->Notify();
  mock_browsing_data_file_system_helper_->AddFileSystemSamples();
  mock_browsing_data_file_system_helper_->Notify();
  mock_browsing_data_quota_helper_->AddQuotaSamples();
  mock_browsing_data_quota_helper_->Notify();
  mock_browsing_data_service_worker_helper_->AddServiceWorkerSamples();
  mock_browsing_data_service_worker_helper_->Notify();

  {
    SCOPED_TRACE("Initial State 4 cookies, 2 databases, 2 local storages, "
                 "2 session storages, 2 indexed DBs, 3 file systems, "
                 "2 quotas, 2 service workers.");
    // 52 because there's the root, then
    // foo1 -> cookies -> a,
    // foo2 -> cookies -> b,
    // foo3 -> cookies -> c,d
    // dbhost1 -> database -> db1,
    // dbhost2 -> database -> db2,
    // host1 -> localstorage -> http://host1:1/,
    //       -> sessionstorage -> http://host1:1/,
    // host2 -> localstorage -> http://host2:2/,
    //       -> sessionstorage -> http://host2:2/,
    // idbhost1 -> sessionstorage -> http://idbhost1:1/,
    // idbhost2 -> sessionstorage -> http://idbhost2:2/,
    // fshost1 -> filesystem -> http://fshost1:1/,
    // fshost2 -> filesystem -> http://fshost2:1/,
    // fshost3 -> filesystem -> http://fshost3:1/,
    // quotahost1 -> quotahost1,
    // quotahost2 -> quotahost2.
    // swhost1 -> service worker -> https://swhost1:1
    // swhost2 -> service worker -> https://swhost1:2
    EXPECT_EQ(52, cookies_model.GetRoot()->GetTotalNodeCount());
    EXPECT_STREQ("A,B,C,D", GetDisplayedCookies(&cookies_model).c_str());
    EXPECT_EQ("db1,db2", GetDisplayedDatabases(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(&cookies_model));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(&cookies_model));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(&cookies_model));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(&cookies_model));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(&cookies_model));
  }
  DeleteStoredObjects(cookies_model.GetRoot()->GetChild(2));
  {
    SCOPED_TRACE("Third origin removed");
    EXPECT_STREQ("A,B", GetDisplayedCookies(&cookies_model).c_str());
    EXPECT_EQ("db1,db2", GetDisplayedDatabases(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(&cookies_model));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(&cookies_model));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(&cookies_model));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(&cookies_model));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(&cookies_model));
    EXPECT_EQ(48, cookies_model.GetRoot()->GetTotalNodeCount());
  }
}

TEST_F(CookiesTreeModelTest, RemoveSingleCookieNodeOf3) {
  LocalDataContainer* container =
      new LocalDataContainer(mock_browsing_data_cookie_helper_.get(),
                             mock_browsing_data_database_helper_.get(),
                             mock_browsing_data_local_storage_helper_.get(),
                             mock_browsing_data_session_storage_helper_.get(),
                             mock_browsing_data_appcache_helper_.get(),
                             mock_browsing_data_indexed_db_helper_.get(),
                             mock_browsing_data_file_system_helper_.get(),
                             mock_browsing_data_quota_helper_.get(),
                             mock_browsing_data_channel_id_helper_.get(),
                             mock_browsing_data_service_worker_helper_.get(),
                             mock_browsing_data_flash_lso_helper_.get());
  CookiesTreeModel cookies_model(container, special_storage_policy(), false);

  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo1"), "A=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo2"), "B=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3"), "C=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3"), "D=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3"), "E=1");
  mock_browsing_data_cookie_helper_->Notify();
  mock_browsing_data_database_helper_->AddDatabaseSamples();
  mock_browsing_data_database_helper_->Notify();
  mock_browsing_data_local_storage_helper_->AddLocalStorageSamples();
  mock_browsing_data_local_storage_helper_->Notify();
  mock_browsing_data_session_storage_helper_->AddLocalStorageSamples();
  mock_browsing_data_session_storage_helper_->Notify();
  mock_browsing_data_indexed_db_helper_->AddIndexedDBSamples();
  mock_browsing_data_indexed_db_helper_->Notify();
  mock_browsing_data_file_system_helper_->AddFileSystemSamples();
  mock_browsing_data_file_system_helper_->Notify();
  mock_browsing_data_quota_helper_->AddQuotaSamples();
  mock_browsing_data_quota_helper_->Notify();
  mock_browsing_data_service_worker_helper_->AddServiceWorkerSamples();
  mock_browsing_data_service_worker_helper_->Notify();

  {
    SCOPED_TRACE("Initial State 5 cookies, 2 databases, 2 local storages, "
                 "2 session storages, 2 indexed DBs, 3 filesystems, "
                 "2 quotas, 2 service workers.");
    // 53 because there's the root, then
    // foo1 -> cookies -> a,
    // foo2 -> cookies -> b,
    // foo3 -> cookies -> c,d,e
    // dbhost1 -> database -> db1,
    // dbhost2 -> database -> db2,
    // host1 -> localstorage -> http://host1:1/,
    //       -> sessionstorage -> http://host1:1/,
    // host2 -> localstorage -> http://host2:2/,
    //       -> sessionstorage -> http://host2:2/,
    // idbhost1 -> sessionstorage -> http://idbhost1:1/,
    // idbhost2 -> sessionstorage -> http://idbhost2:2/,
    // fshost1 -> filesystem -> http://fshost1:1/,
    // fshost2 -> filesystem -> http://fshost2:1/,
    // fshost3 -> filesystem -> http://fshost3:1/,
    // quotahost1 -> quotahost1,
    // quotahost2 -> quotahost2.
    // swhost1 -> service worker -> https://swhost1:1
    // swhost2 -> service worker -> https://swhost1:2
    EXPECT_EQ(53, cookies_model.GetRoot()->GetTotalNodeCount());
    EXPECT_STREQ("A,B,C,D,E", GetDisplayedCookies(&cookies_model).c_str());
    EXPECT_EQ("db1,db2", GetDisplayedDatabases(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(&cookies_model));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(&cookies_model));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(&cookies_model));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(&cookies_model));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(&cookies_model));
  }
  DeleteStoredObjects(cookies_model.GetRoot()->GetChild(2)->GetChild(0)->
      GetChild(1));
  {
    SCOPED_TRACE("Middle cookie in third origin removed");
    EXPECT_STREQ("A,B,C,E", GetDisplayedCookies(&cookies_model).c_str());
    EXPECT_EQ(52, cookies_model.GetRoot()->GetTotalNodeCount());
    EXPECT_EQ("db1,db2", GetDisplayedDatabases(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(&cookies_model));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(&cookies_model));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(&cookies_model));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(&cookies_model));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(&cookies_model));
  }
}

TEST_F(CookiesTreeModelTest, RemoveSecondOrigin) {
  LocalDataContainer* container =
      new LocalDataContainer(mock_browsing_data_cookie_helper_.get(),
                             mock_browsing_data_database_helper_.get(),
                             mock_browsing_data_local_storage_helper_.get(),
                             mock_browsing_data_session_storage_helper_.get(),
                             mock_browsing_data_appcache_helper_.get(),
                             mock_browsing_data_indexed_db_helper_.get(),
                             mock_browsing_data_file_system_helper_.get(),
                             mock_browsing_data_quota_helper_.get(),
                             mock_browsing_data_channel_id_helper_.get(),
                             mock_browsing_data_service_worker_helper_.get(),
                             mock_browsing_data_flash_lso_helper_.get());
  CookiesTreeModel cookies_model(container, special_storage_policy(), false);

  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo1"), "A=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo2"), "B=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3"), "C=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3"), "D=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3"), "E=1");
  mock_browsing_data_cookie_helper_->Notify();

  {
    SCOPED_TRACE("Initial State 5 cookies");
    // 12 because there's the root, then foo1 -> cookies -> a,
    // foo2 -> cookies -> b, foo3 -> cookies -> c,d,e
    EXPECT_EQ(12, cookies_model.GetRoot()->GetTotalNodeCount());
    EXPECT_STREQ("A,B,C,D,E", GetDisplayedCookies(&cookies_model).c_str());
  }
  DeleteStoredObjects(cookies_model.GetRoot()->GetChild(1));
  {
    SCOPED_TRACE("Second origin removed");
    EXPECT_STREQ("A,C,D,E", GetDisplayedCookies(&cookies_model).c_str());
    // Left with root -> foo1 -> cookies -> a, foo3 -> cookies -> c,d,e
    EXPECT_EQ(9, cookies_model.GetRoot()->GetTotalNodeCount());
  }
}

TEST_F(CookiesTreeModelTest, OriginOrdering) {
  LocalDataContainer* container =
      new LocalDataContainer(mock_browsing_data_cookie_helper_.get(),
                             mock_browsing_data_database_helper_.get(),
                             mock_browsing_data_local_storage_helper_.get(),
                             mock_browsing_data_session_storage_helper_.get(),
                             mock_browsing_data_appcache_helper_.get(),
                             mock_browsing_data_indexed_db_helper_.get(),
                             mock_browsing_data_file_system_helper_.get(),
                             mock_browsing_data_quota_helper_.get(),
                             mock_browsing_data_channel_id_helper_.get(),
                             mock_browsing_data_service_worker_helper_.get(),
                             mock_browsing_data_flash_lso_helper_.get());
  CookiesTreeModel cookies_model(container, special_storage_policy(), false);

  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://a.foo2.com"), "A=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo2.com"), "B=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://b.foo1.com"), "C=1");
  // Leading dot on the foo4
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("http://foo4.com"), "D=1; domain=.foo4.com; path=/;");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://a.foo1.com"), "E=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo1.com"), "F=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3.com"), "G=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo4.com"), "H=1");
  mock_browsing_data_cookie_helper_->Notify();

  {
    SCOPED_TRACE("Initial State 8 cookies");
    EXPECT_EQ(23, cookies_model.GetRoot()->GetTotalNodeCount());
    EXPECT_STREQ("F,E,C,B,A,G,D,H",
        GetDisplayedCookies(&cookies_model).c_str());
  }
  // Delete "E"
  DeleteStoredObjects(cookies_model.GetRoot()->GetChild(1));
  {
    EXPECT_STREQ("F,C,B,A,G,D,H", GetDisplayedCookies(&cookies_model).c_str());
  }
}

TEST_F(CookiesTreeModelTest, ContentSettings) {
  GURL host("http://xyz.com/");
  LocalDataContainer* container =
      new LocalDataContainer(mock_browsing_data_cookie_helper_.get(),
                             mock_browsing_data_database_helper_.get(),
                             mock_browsing_data_local_storage_helper_.get(),
                             mock_browsing_data_session_storage_helper_.get(),
                             mock_browsing_data_appcache_helper_.get(),
                             mock_browsing_data_indexed_db_helper_.get(),
                             mock_browsing_data_file_system_helper_.get(),
                             mock_browsing_data_quota_helper_.get(),
                             mock_browsing_data_channel_id_helper_.get(),
                             mock_browsing_data_service_worker_helper_.get(),
                             mock_browsing_data_flash_lso_helper_.get());
  CookiesTreeModel cookies_model(container, special_storage_policy(), false);

  mock_browsing_data_cookie_helper_->AddCookieSamples(host, "A=1");
  mock_browsing_data_cookie_helper_->Notify();

  TestingProfile profile;
  HostContentSettingsMap* content_settings =
      profile.GetHostContentSettingsMap();
  CookieSettings* cookie_settings =
      CookieSettings::Factory::GetForProfile(&profile).get();
  MockSettingsObserver observer(content_settings);

  CookieTreeRootNode* root =
      static_cast<CookieTreeRootNode*>(cookies_model.GetRoot());
  CookieTreeHostNode* origin =
      root->GetOrCreateHostNode(host);

  EXPECT_EQ(1, origin->child_count());
  EXPECT_TRUE(origin->CanCreateContentException());
  EXPECT_CALL(observer,
              OnContentSettingsChanged(
                  content_settings,
                  CONTENT_SETTINGS_TYPE_COOKIES,
                  false,
                  ContentSettingsPattern::FromURLNoWildcard(host),
                  ContentSettingsPattern::Wildcard(),
                  false));
  EXPECT_CALL(observer,
              OnContentSettingsChanged(content_settings,
                  CONTENT_SETTINGS_TYPE_COOKIES,
                  false,
                  ContentSettingsPattern::FromURL(host),
                  ContentSettingsPattern::Wildcard(),
                  false));
  origin->CreateContentException(
      cookie_settings, CONTENT_SETTING_SESSION_ONLY);
  EXPECT_TRUE(cookie_settings->IsReadingCookieAllowed(host, host));
  EXPECT_TRUE(cookie_settings->IsCookieSessionOnly(host));
}

TEST_F(CookiesTreeModelTest, FileSystemFilter) {
  scoped_ptr<CookiesTreeModel> cookies_model(
      CreateCookiesTreeModelWithInitialSample());

  cookies_model->UpdateSearchResults(base::ASCIIToUTF16("fshost1"));
  EXPECT_EQ("http://fshost1:1/",
            GetDisplayedFileSystems(cookies_model.get()));

  cookies_model->UpdateSearchResults(base::ASCIIToUTF16("fshost2"));
  EXPECT_EQ("http://fshost2:2/",
            GetDisplayedFileSystems(cookies_model.get()));

  cookies_model->UpdateSearchResults(base::ASCIIToUTF16("fshost3"));
  EXPECT_EQ("http://fshost3:3/",
            GetDisplayedFileSystems(cookies_model.get()));

  cookies_model->UpdateSearchResults(base::string16());
  EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
            GetDisplayedFileSystems(cookies_model.get()));
}

TEST_F(CookiesTreeModelTest, ServiceWorkerFilter) {
  scoped_ptr<CookiesTreeModel> cookies_model(
      CreateCookiesTreeModelWithInitialSample());

  cookies_model->UpdateSearchResults(base::ASCIIToUTF16("swhost1"));
  EXPECT_EQ("https://swhost1:1/",
            GetDisplayedServiceWorkers(cookies_model.get()));

  cookies_model->UpdateSearchResults(base::ASCIIToUTF16("swhost2"));
  EXPECT_EQ("https://swhost2:2/",
            GetDisplayedServiceWorkers(cookies_model.get()));

  cookies_model->UpdateSearchResults(base::ASCIIToUTF16("swhost3"));
  EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));

  cookies_model->UpdateSearchResults(base::string16());
  EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
            GetDisplayedServiceWorkers(cookies_model.get()));
}

TEST_F(CookiesTreeModelTest, CookiesFilter) {
  LocalDataContainer* container =
      new LocalDataContainer(mock_browsing_data_cookie_helper_.get(),
                             mock_browsing_data_database_helper_.get(),
                             mock_browsing_data_local_storage_helper_.get(),
                             mock_browsing_data_session_storage_helper_.get(),
                             mock_browsing_data_appcache_helper_.get(),
                             mock_browsing_data_indexed_db_helper_.get(),
                             mock_browsing_data_file_system_helper_.get(),
                             mock_browsing_data_quota_helper_.get(),
                             mock_browsing_data_channel_id_helper_.get(),
                             mock_browsing_data_service_worker_helper_.get(),
                             mock_browsing_data_flash_lso_helper_.get());
  CookiesTreeModel cookies_model(container, special_storage_policy(), false);

  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://123.com"), "A=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo1.com"), "B=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo2.com"), "C=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3.com"), "D=1");
  mock_browsing_data_cookie_helper_->Notify();
  EXPECT_EQ("A,B,C,D", GetDisplayedCookies(&cookies_model));

  cookies_model.UpdateSearchResults(base::string16(base::ASCIIToUTF16("foo")));
  EXPECT_EQ("B,C,D", GetDisplayedCookies(&cookies_model));

  cookies_model.UpdateSearchResults(base::string16(base::ASCIIToUTF16("2")));
  EXPECT_EQ("A,C", GetDisplayedCookies(&cookies_model));

  cookies_model.UpdateSearchResults(base::string16(base::ASCIIToUTF16("foo3")));
  EXPECT_EQ("D", GetDisplayedCookies(&cookies_model));

  cookies_model.UpdateSearchResults(base::string16());
  EXPECT_EQ("A,B,C,D", GetDisplayedCookies(&cookies_model));
}

}  // namespace
