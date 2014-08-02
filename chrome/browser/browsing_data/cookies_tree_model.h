// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_COOKIES_TREE_MODEL_H_
#define CHROME_BROWSER_BROWSING_DATA_COOKIES_TREE_MODEL_H_

// TODO(viettrungluu): This header file #includes far too much and has too much
// inline code (which shouldn't be inline).

#include <list>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browsing_data/browsing_data_appcache_helper.h"
#include "chrome/browser/browsing_data/browsing_data_database_helper.h"
#include "chrome/browser/browsing_data/browsing_data_file_system_helper.h"
#include "chrome/browser/browsing_data/browsing_data_indexed_db_helper.h"
#include "chrome/browser/browsing_data/browsing_data_local_storage_helper.h"
#include "chrome/browser/browsing_data/browsing_data_quota_helper.h"
#include "chrome/browser/browsing_data/local_data_container.h"
#include "chrome/common/content_settings.h"
#include "net/ssl/channel_id_store.h"
#include "ui/base/models/tree_node_model.h"

class BrowsingDataChannelIDHelper;
class BrowsingDataCookieHelper;
class CookieSettings;
class CookiesTreeModel;
class CookieTreeAppCacheNode;
class CookieTreeAppCachesNode;
class CookieTreeChannelIDNode;
class CookieTreeChannelIDsNode;
class CookieTreeCookieNode;
class CookieTreeCookiesNode;
class CookieTreeDatabaseNode;
class CookieTreeDatabasesNode;
class CookieTreeFileSystemNode;
class CookieTreeFileSystemsNode;
class CookieTreeFlashLSONode;
class CookieTreeHostNode;
class CookieTreeIndexedDBNode;
class CookieTreeIndexedDBsNode;
class CookieTreeLocalStorageNode;
class CookieTreeLocalStoragesNode;
class CookieTreeQuotaNode;
class CookieTreeSessionStorageNode;
class CookieTreeSessionStoragesNode;
class ExtensionSpecialStoragePolicy;

namespace extensions {
class ExtensionSet;
}

namespace net {
class CanonicalCookie;
}

// CookieTreeNode -------------------------------------------------------------
// The base node type in the Cookies, Databases, and Local Storage options
// view, from which all other types are derived. Specialized from TreeNode in
// that it has a notion of deleting objects stored in the profile, and being
// able to have its children do the same.
class CookieTreeNode : public ui::TreeNode<CookieTreeNode> {
 public:
  // Used to pull out information for the InfoView (the details display below
  // the tree control.)
  struct DetailedInfo {
    // NodeType corresponds to the various CookieTreeNode types.
    enum NodeType {
      TYPE_NONE,
      TYPE_ROOT,  // This is used for CookieTreeRootNode nodes.
      TYPE_HOST,  // This is used for CookieTreeHostNode nodes.
      TYPE_COOKIES,  // This is used for CookieTreeCookiesNode nodes.
      TYPE_COOKIE,  // This is used for CookieTreeCookieNode nodes.
      TYPE_DATABASES,  // This is used for CookieTreeDatabasesNode.
      TYPE_DATABASE,  // This is used for CookieTreeDatabaseNode.
      TYPE_LOCAL_STORAGES,  // This is used for CookieTreeLocalStoragesNode.
      TYPE_LOCAL_STORAGE,  // This is used for CookieTreeLocalStorageNode.
      TYPE_SESSION_STORAGES,  // This is used for CookieTreeSessionStoragesNode.
      TYPE_SESSION_STORAGE,  // This is used for CookieTreeSessionStorageNode.
      TYPE_APPCACHES,  // This is used for CookieTreeAppCachesNode.
      TYPE_APPCACHE,  // This is used for CookieTreeAppCacheNode.
      TYPE_INDEXED_DBS,  // This is used for CookieTreeIndexedDBsNode.
      TYPE_INDEXED_DB,  // This is used for CookieTreeIndexedDBNode.
      TYPE_FILE_SYSTEMS,  // This is used for CookieTreeFileSystemsNode.
      TYPE_FILE_SYSTEM,  // This is used for CookieTreeFileSystemNode.
      TYPE_QUOTA,  // This is used for CookieTreeQuotaNode.
      TYPE_CHANNEL_IDS, // Used for CookieTreeChannelIDsNode.
      TYPE_CHANNEL_ID, // Used for CookieTreeChannelIDNode.
      TYPE_FLASH_LSO,  // This is used for CookieTreeFlashLSONode.
    };

    DetailedInfo();
    ~DetailedInfo();

    DetailedInfo& Init(NodeType type);
    DetailedInfo& InitHost();
    DetailedInfo& InitCookie(const net::CanonicalCookie* cookie);
    DetailedInfo& InitDatabase(
        const BrowsingDataDatabaseHelper::DatabaseInfo* database_info);
    DetailedInfo& InitLocalStorage(
        const BrowsingDataLocalStorageHelper::LocalStorageInfo*
        local_storage_info);
    DetailedInfo& InitSessionStorage(
        const BrowsingDataLocalStorageHelper::LocalStorageInfo*
        session_storage_info);
    DetailedInfo& InitAppCache(const GURL& origin,
                               const content::AppCacheInfo* appcache_info);
    DetailedInfo& InitIndexedDB(
        const content::IndexedDBInfo* indexed_db_info);
    DetailedInfo& InitFileSystem(
        const BrowsingDataFileSystemHelper::FileSystemInfo* file_system_info);
    DetailedInfo& InitQuota(
        const BrowsingDataQuotaHelper::QuotaInfo* quota_info);
    DetailedInfo& InitChannelID(
        const net::ChannelIDStore::ChannelID* channel_id);
    DetailedInfo& InitFlashLSO(const std::string& flash_lso_domain);

    NodeType node_type;
    GURL origin;
    const net::CanonicalCookie* cookie;
    const BrowsingDataDatabaseHelper::DatabaseInfo* database_info;
    const BrowsingDataLocalStorageHelper::LocalStorageInfo* local_storage_info;
    const BrowsingDataLocalStorageHelper::LocalStorageInfo*
        session_storage_info;
    const content::AppCacheInfo* appcache_info;
    const content::IndexedDBInfo* indexed_db_info;
    const BrowsingDataFileSystemHelper::FileSystemInfo* file_system_info;
    const BrowsingDataQuotaHelper::QuotaInfo* quota_info;
    const net::ChannelIDStore::ChannelID* channel_id;
    std::string flash_lso_domain;
  };

  CookieTreeNode() {}
  explicit CookieTreeNode(const base::string16& title)
      : ui::TreeNode<CookieTreeNode>(title) {}
  virtual ~CookieTreeNode() {}

  // Delete backend storage for this node, and any children nodes. (E.g. delete
  // the cookie from CookieMonster, clear the database, and so forth.)
  virtual void DeleteStoredObjects();

  // Gets a pointer back to the associated model for the tree we are in.
  virtual CookiesTreeModel* GetModel() const;

  // Returns a struct with detailed information used to populate the details
  // part of the view.
  virtual DetailedInfo GetDetailedInfo() const = 0;

 protected:
  void AddChildSortedByTitle(CookieTreeNode* new_child);

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieTreeNode);
};

// CookieTreeRootNode ---------------------------------------------------------
// The node at the root of the CookieTree that gets inserted into the view.
class CookieTreeRootNode : public CookieTreeNode {
 public:
  explicit CookieTreeRootNode(CookiesTreeModel* model);
  virtual ~CookieTreeRootNode();

  CookieTreeHostNode* GetOrCreateHostNode(const GURL& url);

  // CookieTreeNode methods:
  virtual CookiesTreeModel* GetModel() const OVERRIDE;
  virtual DetailedInfo GetDetailedInfo() const OVERRIDE;

 private:
  CookiesTreeModel* model_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeRootNode);
};

// CookieTreeHostNode -------------------------------------------------------
class CookieTreeHostNode : public CookieTreeNode {
 public:
  // Returns the host node's title to use for a given URL.
  static base::string16 TitleForUrl(const GURL& url);

  explicit CookieTreeHostNode(const GURL& url);
  virtual ~CookieTreeHostNode();

  // CookieTreeNode methods:
  virtual DetailedInfo GetDetailedInfo() const OVERRIDE;

  // CookieTreeHostNode methods:
  CookieTreeCookiesNode* GetOrCreateCookiesNode();
  CookieTreeDatabasesNode* GetOrCreateDatabasesNode();
  CookieTreeLocalStoragesNode* GetOrCreateLocalStoragesNode();
  CookieTreeSessionStoragesNode* GetOrCreateSessionStoragesNode();
  CookieTreeAppCachesNode* GetOrCreateAppCachesNode();
  CookieTreeIndexedDBsNode* GetOrCreateIndexedDBsNode();
  CookieTreeFileSystemsNode* GetOrCreateFileSystemsNode();
  CookieTreeChannelIDsNode* GetOrCreateChannelIDsNode();
  CookieTreeQuotaNode* UpdateOrCreateQuotaNode(
      std::list<BrowsingDataQuotaHelper::QuotaInfo>::iterator quota_info);
  CookieTreeFlashLSONode* GetOrCreateFlashLSONode(const std::string& domain);

  std::string canonicalized_host() const { return canonicalized_host_; }

  // Creates an content exception for this origin of type
  // CONTENT_SETTINGS_TYPE_COOKIES.
  void CreateContentException(CookieSettings* cookie_settings,
                              ContentSetting setting) const;

  // True if a content exception can be created for this origin.
  bool CanCreateContentException() const;

  const std::string GetHost() const;

 private:
  // Pointers to the cookies, databases, local and session storage and appcache
  // nodes.  When we build up the tree we need to quickly get a reference to
  // the COOKIES node to add children. Checking each child and interrogating
  // them to see if they are a COOKIES, APPCACHES, DATABASES etc node seems
  // less preferable than storing an extra pointer per origin.
  CookieTreeCookiesNode* cookies_child_;
  CookieTreeDatabasesNode* databases_child_;
  CookieTreeLocalStoragesNode* local_storages_child_;
  CookieTreeSessionStoragesNode* session_storages_child_;
  CookieTreeAppCachesNode* appcaches_child_;
  CookieTreeIndexedDBsNode* indexed_dbs_child_;
  CookieTreeFileSystemsNode* file_systems_child_;
  CookieTreeQuotaNode* quota_child_;
  CookieTreeChannelIDsNode* channel_ids_child_;
  CookieTreeFlashLSONode* flash_lso_child_;

  // The URL for which this node was initially created.
  GURL url_;

  std::string canonicalized_host_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeHostNode);
};

// CookieTreeCookieNode ------------------------------------------------------
class CookieTreeCookieNode : public CookieTreeNode {
 public:
  friend class CookieTreeCookiesNode;

  // The cookie should remain valid at least as long as the
  // CookieTreeCookieNode is valid.
  explicit CookieTreeCookieNode(
      std::list<net::CanonicalCookie>::iterator cookie);
  virtual ~CookieTreeCookieNode();

  // CookieTreeNode methods:
  virtual void DeleteStoredObjects() OVERRIDE;
  virtual DetailedInfo GetDetailedInfo() const OVERRIDE;

 private:
  // cookie_ is expected to remain valid as long as the CookieTreeCookieNode is
  // valid.
  std::list<net::CanonicalCookie>::iterator cookie_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeCookieNode);
};

class CookieTreeCookiesNode : public CookieTreeNode {
 public:
  CookieTreeCookiesNode();
  virtual ~CookieTreeCookiesNode();

  virtual DetailedInfo GetDetailedInfo() const OVERRIDE;

  void AddCookieNode(CookieTreeCookieNode* child) {
    AddChildSortedByTitle(child);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieTreeCookiesNode);
};

// CookieTreeAppCacheNode -----------------------------------------------------
class CookieTreeAppCacheNode : public CookieTreeNode {
 public:
  friend class CookieTreeAppCachesNode;

  // appcache_info should remain valid at least as long as the
  // CookieTreeAppCacheNode is valid.
  explicit CookieTreeAppCacheNode(
      const GURL& origin_url,
      std::list<content::AppCacheInfo>::iterator appcache_info);
  virtual ~CookieTreeAppCacheNode();

  virtual void DeleteStoredObjects() OVERRIDE;
  virtual DetailedInfo GetDetailedInfo() const OVERRIDE;

 private:
  GURL origin_url_;
  std::list<content::AppCacheInfo>::iterator appcache_info_;
  DISALLOW_COPY_AND_ASSIGN(CookieTreeAppCacheNode);
};

class CookieTreeAppCachesNode : public CookieTreeNode {
 public:
  CookieTreeAppCachesNode();
  virtual ~CookieTreeAppCachesNode();

  virtual DetailedInfo GetDetailedInfo() const OVERRIDE;

  void AddAppCacheNode(CookieTreeAppCacheNode* child) {
    AddChildSortedByTitle(child);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieTreeAppCachesNode);
};

// CookieTreeDatabaseNode -----------------------------------------------------
class CookieTreeDatabaseNode : public CookieTreeNode {
 public:
  friend class CookieTreeDatabasesNode;

  // database_info should remain valid at least as long as the
  // CookieTreeDatabaseNode is valid.
  explicit CookieTreeDatabaseNode(
      std::list<BrowsingDataDatabaseHelper::DatabaseInfo>::iterator
          database_info);
  virtual ~CookieTreeDatabaseNode();

  virtual void DeleteStoredObjects() OVERRIDE;
  virtual DetailedInfo GetDetailedInfo() const OVERRIDE;

 private:
  // database_info_ is expected to remain valid as long as the
  // CookieTreeDatabaseNode is valid.
  std::list<BrowsingDataDatabaseHelper::DatabaseInfo>::iterator
      database_info_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeDatabaseNode);
};

class CookieTreeDatabasesNode : public CookieTreeNode {
 public:
  CookieTreeDatabasesNode();
  virtual ~CookieTreeDatabasesNode();

  virtual DetailedInfo GetDetailedInfo() const OVERRIDE;

  void AddDatabaseNode(CookieTreeDatabaseNode* child) {
    AddChildSortedByTitle(child);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieTreeDatabasesNode);
};

// CookieTreeFileSystemNode --------------------------------------------------
class CookieTreeFileSystemNode : public CookieTreeNode {
 public:
  friend class CookieTreeFileSystemsNode;

  // file_system_info should remain valid at least as long as the
  // CookieTreeFileSystemNode is valid.
  explicit CookieTreeFileSystemNode(
      std::list<BrowsingDataFileSystemHelper::FileSystemInfo>::iterator
          file_system_info);
  virtual ~CookieTreeFileSystemNode();

  virtual void DeleteStoredObjects() OVERRIDE;
  virtual DetailedInfo GetDetailedInfo() const OVERRIDE;

 private:
  // file_system_info_ expected to remain valid as long as the
  // CookieTreeFileSystemNode is valid.
  std::list<BrowsingDataFileSystemHelper::FileSystemInfo>::iterator
      file_system_info_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeFileSystemNode);
};

class CookieTreeFileSystemsNode : public CookieTreeNode {
 public:
  CookieTreeFileSystemsNode();
  virtual ~CookieTreeFileSystemsNode();

  virtual DetailedInfo GetDetailedInfo() const OVERRIDE;

  void AddFileSystemNode(CookieTreeFileSystemNode* child) {
    AddChildSortedByTitle(child);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieTreeFileSystemsNode);
};

// CookieTreeLocalStorageNode -------------------------------------------------
class CookieTreeLocalStorageNode : public CookieTreeNode {
 public:
  // local_storage_info should remain valid at least as long as the
  // CookieTreeLocalStorageNode is valid.
  explicit CookieTreeLocalStorageNode(
      std::list<BrowsingDataLocalStorageHelper::LocalStorageInfo>::iterator
          local_storage_info);
  virtual ~CookieTreeLocalStorageNode();

  // CookieTreeNode methods:
  virtual void DeleteStoredObjects() OVERRIDE;
  virtual DetailedInfo GetDetailedInfo() const OVERRIDE;

 private:
  // local_storage_info_ is expected to remain valid as long as the
  // CookieTreeLocalStorageNode is valid.
  std::list<BrowsingDataLocalStorageHelper::LocalStorageInfo>::iterator
      local_storage_info_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeLocalStorageNode);
};

class CookieTreeLocalStoragesNode : public CookieTreeNode {
 public:
  CookieTreeLocalStoragesNode();
  virtual ~CookieTreeLocalStoragesNode();

  virtual DetailedInfo GetDetailedInfo() const OVERRIDE;

  void AddLocalStorageNode(CookieTreeLocalStorageNode* child) {
    AddChildSortedByTitle(child);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieTreeLocalStoragesNode);
};


// CookieTreeSessionStorageNode -----------------------------------------------
class CookieTreeSessionStorageNode : public CookieTreeNode {
 public:
  // session_storage_info should remain valid at least as long as the
  // CookieTreeSessionStorageNode is valid.
  explicit CookieTreeSessionStorageNode(
      std::list<BrowsingDataLocalStorageHelper::LocalStorageInfo>::iterator
          session_storage_info);
  virtual ~CookieTreeSessionStorageNode();

  // CookieTreeNode methods:
  virtual void DeleteStoredObjects() OVERRIDE;
  virtual DetailedInfo GetDetailedInfo() const OVERRIDE;

 private:
  // session_storage_info_ is expected to remain valid as long as the
  // CookieTreeSessionStorageNode is valid.
  std::list<BrowsingDataLocalStorageHelper::LocalStorageInfo>::iterator
      session_storage_info_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeSessionStorageNode);
};

class CookieTreeSessionStoragesNode : public CookieTreeNode {
 public:
  CookieTreeSessionStoragesNode();
  virtual ~CookieTreeSessionStoragesNode();

  virtual DetailedInfo GetDetailedInfo() const OVERRIDE;

  void AddSessionStorageNode(CookieTreeSessionStorageNode* child) {
    AddChildSortedByTitle(child);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieTreeSessionStoragesNode);
};

// CookieTreeIndexedDBNode -----------------------------------------------
class CookieTreeIndexedDBNode : public CookieTreeNode {
 public:
  // indexed_db_info should remain valid at least as long as the
  // CookieTreeIndexedDBNode is valid.
  explicit CookieTreeIndexedDBNode(
      std::list<content::IndexedDBInfo>::iterator
          indexed_db_info);
  virtual ~CookieTreeIndexedDBNode();

  // CookieTreeNode methods:
  virtual void DeleteStoredObjects() OVERRIDE;
  virtual DetailedInfo GetDetailedInfo() const OVERRIDE;

 private:
  // indexed_db_info_ is expected to remain valid as long as the
  // CookieTreeIndexedDBNode is valid.
  std::list<content::IndexedDBInfo>::iterator
      indexed_db_info_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeIndexedDBNode);
};

class CookieTreeIndexedDBsNode : public CookieTreeNode {
 public:
  CookieTreeIndexedDBsNode();
  virtual ~CookieTreeIndexedDBsNode();

  virtual DetailedInfo GetDetailedInfo() const OVERRIDE;

  void AddIndexedDBNode(CookieTreeIndexedDBNode* child) {
    AddChildSortedByTitle(child);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieTreeIndexedDBsNode);
};

// CookieTreeQuotaNode --------------------------------------------------
class CookieTreeQuotaNode : public CookieTreeNode {
 public:
  // quota_info should remain valid at least as long as the CookieTreeQuotaNode
  // is valid.
  explicit CookieTreeQuotaNode(
      std::list<BrowsingDataQuotaHelper::QuotaInfo>::iterator quota_info);
  virtual ~CookieTreeQuotaNode();

  virtual void DeleteStoredObjects() OVERRIDE;
  virtual DetailedInfo GetDetailedInfo() const OVERRIDE;

 private:
  // quota_info_ is expected to remain valid as long as the CookieTreeQuotaNode
  // is valid.
  std::list<BrowsingDataQuotaHelper::QuotaInfo>::iterator quota_info_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeQuotaNode);
};

// CookieTreeChannelIDNode ---------------------------------------------
class CookieTreeChannelIDNode : public CookieTreeNode {
 public:
  friend class CookieTreeChannelIDsNode;

  // The iterator should remain valid at least as long as the
  // CookieTreeChannelIDNode is valid.
  explicit CookieTreeChannelIDNode(
      net::ChannelIDStore::ChannelIDList::iterator cert);
  virtual ~CookieTreeChannelIDNode();

  // CookieTreeNode methods:
  virtual void DeleteStoredObjects() OVERRIDE;
  virtual DetailedInfo GetDetailedInfo() const OVERRIDE;

 private:
  // channel_id_ is expected to remain valid as long as the
  // CookieTreeChannelIDNode is valid.
  net::ChannelIDStore::ChannelIDList::iterator channel_id_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeChannelIDNode);
};

class CookieTreeChannelIDsNode : public CookieTreeNode {
 public:
  CookieTreeChannelIDsNode();
  virtual ~CookieTreeChannelIDsNode();

  virtual DetailedInfo GetDetailedInfo() const OVERRIDE;

  void AddChannelIDNode(CookieTreeChannelIDNode* child) {
    AddChildSortedByTitle(child);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieTreeChannelIDsNode);
};

// CookieTreeFlashLSONode ----------------------------------------------------
class CookieTreeFlashLSONode : public CookieTreeNode {
 public:
  explicit CookieTreeFlashLSONode(const std::string& domain);
  virtual ~CookieTreeFlashLSONode();

  // CookieTreeNode methods:
  virtual void DeleteStoredObjects() OVERRIDE;
  virtual DetailedInfo GetDetailedInfo() const OVERRIDE;

 private:
  std::string domain_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeFlashLSONode);
};

// CookiesTreeModel -----------------------------------------------------------
class CookiesTreeModel : public ui::TreeNodeModel<CookieTreeNode> {
 public:
  CookiesTreeModel(LocalDataContainer* data_container,
                   ExtensionSpecialStoragePolicy* special_storage_policy,
                   bool group_by_cookie_source);
  virtual ~CookiesTreeModel();

  // Because non-cookie nodes are fetched in a background thread, they are not
  // present at the time the Model is created. The Model then notifies its
  // observers for every item added from databases, local storage, and
  // appcache. We extend the Observer interface to add notifications before and
  // after these batch inserts.
  class Observer : public ui::TreeModelObserver {
   public:
    virtual void TreeModelBeginBatch(CookiesTreeModel* model) {}
    virtual void TreeModelEndBatch(CookiesTreeModel* model) {}
  };

  // This class defines the scope for batch updates. It can be created as a
  // local variable and the destructor will terminate the batch update, if one
  // has been started.
  class ScopedBatchUpdateNotifier {
   public:
    ScopedBatchUpdateNotifier(CookiesTreeModel* model,
                              CookieTreeNode* node);
    ~ScopedBatchUpdateNotifier();

    void StartBatchUpdate();

   private:
    CookiesTreeModel* model_;
    CookieTreeNode* node_;
    bool batch_in_progress_;
  };

  // ui::TreeModel methods:
  // Returns the set of icons for the nodes in the tree. You only need override
  // this if you don't want to use the default folder icons.
  virtual void GetIcons(std::vector<gfx::ImageSkia>* icons) OVERRIDE;

  // Returns the index of the icon to use for |node|. Return -1 to use the
  // default icon. The index is relative to the list of icons returned from
  // GetIcons.
  virtual int GetIconIndex(ui::TreeModelNode* node) OVERRIDE;

  // CookiesTreeModel methods:
  void DeleteAllStoredObjects();

  // Deletes a specific node in the tree, identified by |cookie_node|, and its
  // subtree.
  void DeleteCookieNode(CookieTreeNode* cookie_node);

  // Filter the origins to only display matched results.
  void UpdateSearchResults(const base::string16& filter);

#if defined(ENABLE_EXTENSIONS)
  // Returns the set of extensions which protect the data item represented by
  // this node from deletion.
  // Returns NULL if the node doesn't represent a protected data item or the
  // special storage policy is NULL.
  const extensions::ExtensionSet* ExtensionsProtectingNode(
      const CookieTreeNode& cookie_node);
#endif

  // Manages CookiesTreeModel::Observers. This will also call
  // TreeNodeModel::AddObserver so that it gets all the proper notifications.
  // Note that the converse is not true: simply adding a TreeModelObserver will
  // not get CookiesTreeModel::Observer notifications.
  virtual void AddCookiesTreeObserver(Observer* observer);
  virtual void RemoveCookiesTreeObserver(Observer* observer);

  // Methods that update the model based on the data retrieved by the browsing
  // data helpers.
  void PopulateAppCacheInfo(LocalDataContainer* container);
  void PopulateCookieInfo(LocalDataContainer* container);
  void PopulateDatabaseInfo(LocalDataContainer* container);
  void PopulateLocalStorageInfo(LocalDataContainer* container);
  void PopulateSessionStorageInfo(LocalDataContainer* container);
  void PopulateIndexedDBInfo(LocalDataContainer* container);
  void PopulateFileSystemInfo(LocalDataContainer* container);
  void PopulateQuotaInfo(LocalDataContainer* container);
  void PopulateChannelIDInfo(LocalDataContainer* container);
  void PopulateFlashLSOInfo(LocalDataContainer* container);

  BrowsingDataCookieHelper* GetCookieHelper(const std::string& app_id);
  LocalDataContainer* data_container() {
    return data_container_.get();
  }

 private:
  enum CookieIconIndex {
    ORIGIN = 0,
    COOKIE = 1,
    DATABASE = 2
  };

  void NotifyObserverBeginBatch();
  void NotifyObserverEndBatch();

  void PopulateAppCacheInfoWithFilter(LocalDataContainer* container,
                                      ScopedBatchUpdateNotifier* notifier,
                                      const base::string16& filter);
  void PopulateCookieInfoWithFilter(LocalDataContainer* container,
                                    ScopedBatchUpdateNotifier* notifier,
                                    const base::string16& filter);
  void PopulateDatabaseInfoWithFilter(LocalDataContainer* container,
                                      ScopedBatchUpdateNotifier* notifier,
                                      const base::string16& filter);
  void PopulateLocalStorageInfoWithFilter(LocalDataContainer* container,
                                          ScopedBatchUpdateNotifier* notifier,
                                          const base::string16& filter);
  void PopulateSessionStorageInfoWithFilter(LocalDataContainer* container,
                                            ScopedBatchUpdateNotifier* notifier,
                                            const base::string16& filter);
  void PopulateIndexedDBInfoWithFilter(LocalDataContainer* container,
                                       ScopedBatchUpdateNotifier* notifier,
                                       const base::string16& filter);
  void PopulateFileSystemInfoWithFilter(LocalDataContainer* container,
                                        ScopedBatchUpdateNotifier* notifier,
                                        const base::string16& filter);
  void PopulateQuotaInfoWithFilter(LocalDataContainer* container,
                                   ScopedBatchUpdateNotifier* notifier,
                                   const base::string16& filter);
  void PopulateChannelIDInfoWithFilter(
      LocalDataContainer* container,
      ScopedBatchUpdateNotifier* notifier,
      const base::string16& filter);
  void PopulateFlashLSOInfoWithFilter(LocalDataContainer* container,
                                      ScopedBatchUpdateNotifier* notifier,
                                      const base::string16& filter);

  // Map of app ids to LocalDataContainer objects to use when retrieving
  // locally stored data.
  scoped_ptr<LocalDataContainer> data_container_;

#if defined(ENABLE_EXTENSIONS)
  // The extension special storage policy; see ExtensionsProtectingNode() above.
  scoped_refptr<ExtensionSpecialStoragePolicy> special_storage_policy_;
#endif

  // The CookiesTreeModel maintains a separate list of observers that are
  // specifically of the type CookiesTreeModel::Observer.
  ObserverList<Observer> cookies_observer_list_;

  // If true, use the CanonicalCookie::Source attribute to group cookies.
  // Otherwise, use the CanonicalCookie::Domain attribute.
  bool group_by_cookie_source_;

  // If this is non-zero, then this model is batching updates (there's a lot of
  // notifications coming down the pipe). This is an integer is used to balance
  // calls to Begin/EndBatch() if they're called in a nested manner.
  int batch_update_;
};

#endif  // CHROME_BROWSER_BROWSING_DATA_COOKIES_TREE_MODEL_H_
