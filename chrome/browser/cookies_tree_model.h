// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COOKIES_TREE_MODEL_H_
#define CHROME_BROWSER_COOKIES_TREE_MODEL_H_
#pragma once

// TODO(viettrungluu): This header file #includes far too much and has too much
// inline code (which shouldn't be inline).

#include <list>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browsing_data_appcache_helper.h"
#include "chrome/browser/browsing_data_database_helper.h"
#include "chrome/browser/browsing_data_file_system_helper.h"
#include "chrome/browser/browsing_data_indexed_db_helper.h"
#include "chrome/browser/browsing_data_local_storage_helper.h"
#include "chrome/browser/browsing_data_quota_helper.h"
#include "chrome/common/content_settings.h"
#include "net/base/cookie_monster.h"
#include "ui/base/models/tree_node_model.h"

class BrowsingDataCookieHelper;
class CookieSettings;
class CookiesTreeModel;
class CookieTreeAppCacheNode;
class CookieTreeAppCachesNode;
class CookieTreeCookieNode;
class CookieTreeCookiesNode;
class CookieTreeDatabaseNode;
class CookieTreeDatabasesNode;
class CookieTreeFileSystemsNode;
class CookieTreeFileSystemNode;
class CookieTreeLocalStorageNode;
class CookieTreeLocalStoragesNode;
class CookieTreeQuotaNode;
class CookieTreeSessionStorageNode;
class CookieTreeSessionStoragesNode;
class CookieTreeIndexedDBNode;
class CookieTreeIndexedDBsNode;
class CookieTreeOriginNode;

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
      TYPE_ROOT,  // This is used for CookieTreeRootNode nodes.
      TYPE_ORIGIN,  // This is used for CookieTreeOriginNode nodes.
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
    };

    // TODO(viettrungluu): Figure out whether we want to store |origin| as a
    // |string16| or a (UTF-8) |std::string|, and convert. Remove constructor
    // taking an |std::wstring|.
    DetailedInfo(const string16& origin, NodeType node_type,
        const net::CookieMonster::CanonicalCookie* cookie,
        const BrowsingDataDatabaseHelper::DatabaseInfo* database_info,
        const BrowsingDataLocalStorageHelper::LocalStorageInfo*
            local_storage_info,
        const BrowsingDataLocalStorageHelper::LocalStorageInfo*
            session_storage_info,
        const appcache::AppCacheInfo* appcache_info,
        const BrowsingDataIndexedDBHelper::IndexedDBInfo* indexed_db_info,
        const BrowsingDataFileSystemHelper::FileSystemInfo* file_system_info,
        const BrowsingDataQuotaHelper::QuotaInfo* quota_info)
        : origin(UTF16ToWideHack(origin)),
          node_type(node_type),
          cookie(cookie),
          database_info(database_info),
          local_storage_info(local_storage_info),
          session_storage_info(session_storage_info),
          appcache_info(appcache_info),
          indexed_db_info(indexed_db_info),
          file_system_info(file_system_info),
          quota_info(quota_info) {
      DCHECK((node_type != TYPE_DATABASE) || database_info);
      DCHECK((node_type != TYPE_LOCAL_STORAGE) || local_storage_info);
      DCHECK((node_type != TYPE_SESSION_STORAGE) || session_storage_info);
      DCHECK((node_type != TYPE_APPCACHE) || appcache_info);
      DCHECK((node_type != TYPE_INDEXED_DB) || indexed_db_info);
      DCHECK((node_type != TYPE_FILE_SYSTEM) || file_system_info);
      DCHECK((node_type != TYPE_QUOTA) || quota_info);
    }
#if !defined(WCHAR_T_IS_UTF16)
    DetailedInfo(const std::wstring& origin, NodeType node_type,
        const net::CookieMonster::CanonicalCookie* cookie,
        const BrowsingDataDatabaseHelper::DatabaseInfo* database_info,
        const BrowsingDataLocalStorageHelper::LocalStorageInfo*
            local_storage_info,
        const BrowsingDataLocalStorageHelper::LocalStorageInfo*
            session_storage_info,
        const appcache::AppCacheInfo* appcache_info,
        const BrowsingDataIndexedDBHelper::IndexedDBInfo* indexed_db_info,
        const BrowsingDataFileSystemHelper::FileSystemInfo* file_system_info,
        const BrowsingDataQuotaHelper::QuotaInfo* quota_info)
        : origin(origin),
          node_type(node_type),
          cookie(cookie),
          database_info(database_info),
          local_storage_info(local_storage_info),
          session_storage_info(session_storage_info),
          appcache_info(appcache_info),
          indexed_db_info(indexed_db_info),
          file_system_info(file_system_info),
          quota_info(quota_info) {
      DCHECK((node_type != TYPE_DATABASE) || database_info);
      DCHECK((node_type != TYPE_LOCAL_STORAGE) || local_storage_info);
      DCHECK((node_type != TYPE_SESSION_STORAGE) || session_storage_info);
      DCHECK((node_type != TYPE_APPCACHE) || appcache_info);
      DCHECK((node_type != TYPE_INDEXED_DB) || indexed_db_info);
      DCHECK((node_type != TYPE_FILE_SYSTEM) || file_system_info);
      DCHECK((node_type != TYPE_QUOTA) || quota_info);
    }
#endif

    std::wstring origin;
    NodeType node_type;
    const net::CookieMonster::CanonicalCookie* cookie;
    const BrowsingDataDatabaseHelper::DatabaseInfo* database_info;
    const BrowsingDataLocalStorageHelper::LocalStorageInfo* local_storage_info;
    const BrowsingDataLocalStorageHelper::LocalStorageInfo*
        session_storage_info;
    const appcache::AppCacheInfo* appcache_info;
    const BrowsingDataIndexedDBHelper::IndexedDBInfo* indexed_db_info;
    const BrowsingDataFileSystemHelper::FileSystemInfo* file_system_info;
    const BrowsingDataQuotaHelper::QuotaInfo* quota_info;
  };

  CookieTreeNode() {}
  explicit CookieTreeNode(const string16& title)
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
  class NodeTitleComparator {
   public:
    bool operator() (const CookieTreeNode* lhs, const CookieTreeNode* rhs);
  };

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

  CookieTreeOriginNode* GetOrCreateOriginNode(const GURL& url);

  // CookieTreeNode methods:
  virtual CookiesTreeModel* GetModel() const OVERRIDE;
  virtual DetailedInfo GetDetailedInfo() const OVERRIDE;

 private:
  CookiesTreeModel* model_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeRootNode);
};

// CookieTreeOriginNode -------------------------------------------------------
class CookieTreeOriginNode : public CookieTreeNode {
 public:
  // Returns the origin node's title to use for a given URL.
  static std::wstring TitleForUrl(const GURL& url);

  explicit CookieTreeOriginNode(const GURL& url);
  virtual ~CookieTreeOriginNode();

  // CookieTreeNode methods:
  virtual DetailedInfo GetDetailedInfo() const OVERRIDE;

  // CookieTreeOriginNode methods:
  CookieTreeCookiesNode* GetOrCreateCookiesNode();
  CookieTreeDatabasesNode* GetOrCreateDatabasesNode();
  CookieTreeLocalStoragesNode* GetOrCreateLocalStoragesNode();
  CookieTreeSessionStoragesNode* GetOrCreateSessionStoragesNode();
  CookieTreeAppCachesNode* GetOrCreateAppCachesNode();
  CookieTreeIndexedDBsNode* GetOrCreateIndexedDBsNode();
  CookieTreeFileSystemsNode* GetOrCreateFileSystemsNode();
  CookieTreeQuotaNode* UpdateOrCreateQuotaNode(
      std::list<BrowsingDataQuotaHelper::QuotaInfo>::iterator quota_info);

  // Creates an content exception for this origin of type
  // CONTENT_SETTINGS_TYPE_COOKIES.
  void CreateContentException(CookieSettings* cookie_settings,
                              ContentSetting setting) const;

  // True if a content exception can be created for this origin.
  bool CanCreateContentException() const;

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

  // The URL for which this node was initially created.
  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeOriginNode);
};

// CookieTreeCookieNode ------------------------------------------------------
class CookieTreeCookieNode : public CookieTreeNode {
 public:
  friend class CookieTreeCookiesNode;

  // The cookie should remain valid at least as long as the
  // CookieTreeCookieNode is valid.
  explicit CookieTreeCookieNode(
      std::list<net::CookieMonster::CanonicalCookie>::iterator cookie);
  virtual ~CookieTreeCookieNode();

  // CookieTreeNode methods:
  virtual void DeleteStoredObjects() OVERRIDE;
  virtual DetailedInfo GetDetailedInfo() const OVERRIDE;

 private:
  // cookie_ is expected to remain valid as long as the CookieTreeCookieNode is
  // valid.
  std::list<net::CookieMonster::CanonicalCookie>::iterator cookie_;

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
      std::list<appcache::AppCacheInfo>::iterator appcache_info);
  virtual ~CookieTreeAppCacheNode();

  virtual void DeleteStoredObjects() OVERRIDE;
  virtual DetailedInfo GetDetailedInfo() const OVERRIDE;

 private:
  GURL origin_url_;
  std::list<appcache::AppCacheInfo>::iterator appcache_info_;
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
      std::list<BrowsingDataIndexedDBHelper::IndexedDBInfo>::iterator
          indexed_db_info);
  virtual ~CookieTreeIndexedDBNode();

  // CookieTreeNode methods:
  virtual void DeleteStoredObjects() OVERRIDE;
  virtual DetailedInfo GetDetailedInfo() const OVERRIDE;

 private:
  // indexed_db_info_ is expected to remain valid as long as the
  // CookieTreeIndexedDBNode is valid.
  std::list<BrowsingDataIndexedDBHelper::IndexedDBInfo>::iterator
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

// CookiesTreeModel -----------------------------------------------------------
class CookiesTreeModel : public ui::TreeNodeModel<CookieTreeNode> {
 public:
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

  CookiesTreeModel(
      BrowsingDataCookieHelper* cookie_helper,
      BrowsingDataDatabaseHelper* database_helper,
      BrowsingDataLocalStorageHelper* local_storage_helper,
      BrowsingDataLocalStorageHelper* session_storage_helper,
      BrowsingDataAppCacheHelper* appcache_helper,
      BrowsingDataIndexedDBHelper* indexed_db_helper,
      BrowsingDataFileSystemHelper* file_system_helper,
      BrowsingDataQuotaHelper* quota_helper,
      bool use_cookie_source);
  virtual ~CookiesTreeModel();

  // ui::TreeModel methods:
  // Returns the set of icons for the nodes in the tree. You only need override
  // this if you don't want to use the default folder icons.
  virtual void GetIcons(std::vector<SkBitmap>* icons) OVERRIDE;

  // Returns the index of the icon to use for |node|. Return -1 to use the
  // default icon. The index is relative to the list of icons returned from
  // GetIcons.
  virtual int GetIconIndex(ui::TreeModelNode* node) OVERRIDE;

  // CookiesTreeModel methods:
  void DeleteAllStoredObjects();
  void DeleteCookieNode(CookieTreeNode* cookie_node);

  // Filter the origins to only display matched results.
  void UpdateSearchResults(const std::wstring& filter);

  // Manages CookiesTreeModel::Observers. This will also call
  // TreeNodeModel::AddObserver so that it gets all the proper notifications.
  // Note that the converse is not true: simply adding a TreeModelObserver will
  // not get CookiesTreeModel::Observer notifications.
  virtual void AddCookiesTreeObserver(Observer* observer);
  virtual void RemoveCookiesTreeObserver(Observer* observer);

 private:
  enum CookieIconIndex {
    ORIGIN = 0,
    COOKIE = 1,
    DATABASE = 2
  };
  typedef std::list<net::CookieMonster::CanonicalCookie> CookieList;
  typedef std::list<BrowsingDataDatabaseHelper::DatabaseInfo>
      DatabaseInfoList;
  typedef std::list<BrowsingDataLocalStorageHelper::LocalStorageInfo>
      LocalStorageInfoList;
  typedef std::list<BrowsingDataLocalStorageHelper::LocalStorageInfo>
      SessionStorageInfoList;
  typedef std::list<BrowsingDataIndexedDBHelper::IndexedDBInfo>
      IndexedDBInfoList;
  typedef std::list<BrowsingDataFileSystemHelper::FileSystemInfo>
      FileSystemInfoList;
  typedef std::list<BrowsingDataQuotaHelper::QuotaInfo> QuotaInfoArray;

  void OnAppCacheModelInfoLoaded();
  void OnCookiesModelInfoLoaded(const net::CookieList& cookie_list);
  void OnDatabaseModelInfoLoaded(const DatabaseInfoList& database_info);
  void OnLocalStorageModelInfoLoaded(
      const LocalStorageInfoList& local_storage_info);
  void OnSessionStorageModelInfoLoaded(
      const LocalStorageInfoList& local_storage_info);
  void OnIndexedDBModelInfoLoaded(
      const IndexedDBInfoList& indexed_db_info);
  void OnFileSystemModelInfoLoaded(
      const FileSystemInfoList& file_system_info);
  void OnQuotaModelInfoLoaded(const QuotaInfoArray& quota_info);

  void PopulateAppCacheInfoWithFilter(const std::wstring& filter);
  void PopulateCookieInfoWithFilter(const std::wstring& filter);
  void PopulateDatabaseInfoWithFilter(const std::wstring& filter);
  void PopulateLocalStorageInfoWithFilter(const std::wstring& filter);
  void PopulateSessionStorageInfoWithFilter(const std::wstring& filter);
  void PopulateIndexedDBInfoWithFilter(const std::wstring& filter);
  void PopulateFileSystemInfoWithFilter(const std::wstring& filter);
  void PopulateQuotaInfoWithFilter(const std::wstring& filter);

  void NotifyObserverBeginBatch();
  void NotifyObserverEndBatch();

  scoped_refptr<BrowsingDataAppCacheHelper> appcache_helper_;
  scoped_refptr<BrowsingDataCookieHelper> cookie_helper_;
  scoped_refptr<BrowsingDataDatabaseHelper> database_helper_;
  scoped_refptr<BrowsingDataLocalStorageHelper> local_storage_helper_;
  scoped_refptr<BrowsingDataLocalStorageHelper> session_storage_helper_;
  scoped_refptr<BrowsingDataIndexedDBHelper> indexed_db_helper_;
  scoped_refptr<BrowsingDataFileSystemHelper> file_system_helper_;
  scoped_refptr<BrowsingDataQuotaHelper> quota_helper_;

  std::map<GURL, std::list<appcache::AppCacheInfo> > appcache_info_;
  CookieList cookie_list_;
  DatabaseInfoList database_info_list_;
  LocalStorageInfoList local_storage_info_list_;
  LocalStorageInfoList session_storage_info_list_;
  IndexedDBInfoList indexed_db_info_list_;
  FileSystemInfoList file_system_info_list_;
  QuotaInfoArray quota_info_list_;

  // The CookiesTreeModel maintains a separate list of observers that are
  // specifically of the type CookiesTreeModel::Observer.
  ObserverList<Observer> cookies_observer_list_;

  // If this is non-zero, then this model is batching updates (there's a lot of
  // notifications coming down the pipe). This is an integer is used to balance
  // calls to Begin/EndBatch() if they're called in a nested manner.
  int batch_update_;

  // If true, use the CanonicalCookie::Source attribute to group cookies.
  // Otherwise, use the CanonicalCookie::Domain attribute.
  bool use_cookie_source_;

  friend class CookieTreeAppCacheNode;
  friend class CookieTreeCookieNode;
  friend class CookieTreeDatabaseNode;
  friend class CookieTreeLocalStorageNode;
  friend class CookieTreeSessionStorageNode;
  friend class CookieTreeIndexedDBNode;
  friend class CookieTreeFileSystemNode;
  friend class CookieTreeQuotaNode;

  DISALLOW_COPY_AND_ASSIGN(CookiesTreeModel);
};

#endif  // CHROME_BROWSER_COOKIES_TREE_MODEL_H_
