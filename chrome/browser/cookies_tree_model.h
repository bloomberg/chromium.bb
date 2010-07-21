// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COOKIES_TREE_MODEL_H_
#define CHROME_BROWSER_COOKIES_TREE_MODEL_H_

#include <string>
#include <vector>

#include "app/tree_node_model.h"
#include "base/observer_list.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/browsing_data_appcache_helper.h"
#include "chrome/browser/browsing_data_database_helper.h"
#include "chrome/browser/browsing_data_local_storage_helper.h"
#include "net/base/cookie_monster.h"

class CookiesTreeModel;
class CookieTreeAppCacheNode;
class CookieTreeAppCachesNode;
class CookieTreeCookieNode;
class CookieTreeCookiesNode;
class CookieTreeDatabaseNode;
class CookieTreeDatabasesNode;
class CookieTreeLocalStorageNode;
class CookieTreeLocalStoragesNode;
class CookieTreeOriginNode;

// CookieTreeNode -------------------------------------------------------------
// The base node type in the Cookies, Databases, and Local Storage options
// view, from which all other types are derived. Specialized from TreeNode in
// that it has a notion of deleting objects stored in the profile, and being
// able to have its children do the same.
class CookieTreeNode : public TreeNode<CookieTreeNode> {
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
      TYPE_APPCACHES,  // This is used for CookieTreeAppCachesNode.
      TYPE_APPCACHE,  // This is used for CookieTreeAppCacheNode.
    };

    DetailedInfo(const std::wstring& origin, NodeType node_type,
        const net::CookieMonster::CanonicalCookie* cookie,
        const BrowsingDataDatabaseHelper::DatabaseInfo* database_info,
        const BrowsingDataLocalStorageHelper::LocalStorageInfo*
            local_storage_info,
        const appcache::AppCacheInfo* appcache_info)
        : origin(origin),
          node_type(node_type),
          cookie(cookie),
          database_info(database_info),
          local_storage_info(local_storage_info),
          appcache_info(appcache_info) {
      DCHECK((node_type != TYPE_DATABASE) || database_info);
      DCHECK((node_type != TYPE_LOCAL_STORAGE) || local_storage_info);
      DCHECK((node_type != TYPE_APPCACHE) || appcache_info);
    }

    std::wstring origin;
    NodeType node_type;
    const net::CookieMonster::CanonicalCookie* cookie;
    const BrowsingDataDatabaseHelper::DatabaseInfo* database_info;
    const BrowsingDataLocalStorageHelper::LocalStorageInfo* local_storage_info;
    const appcache::AppCacheInfo* appcache_info;
  };

  CookieTreeNode() {}
  explicit CookieTreeNode(const std::wstring& title)
      : TreeNode<CookieTreeNode>(title) {}
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
  explicit CookieTreeRootNode(CookiesTreeModel* model) : model_(model) {}
  virtual ~CookieTreeRootNode() {}

  CookieTreeOriginNode* GetOrCreateOriginNode(const GURL& url);

  // CookieTreeNode methods:
  virtual CookiesTreeModel* GetModel() const { return model_; }
  virtual DetailedInfo GetDetailedInfo() const {
    return DetailedInfo(std::wstring(), DetailedInfo::TYPE_ROOT, NULL, NULL,
                        NULL, NULL);
  }
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
  virtual ~CookieTreeOriginNode() {}

  // CookieTreeNode methods:
  virtual DetailedInfo GetDetailedInfo() const {
    return DetailedInfo(GetTitle(), DetailedInfo::TYPE_ORIGIN, NULL, NULL,
                        NULL, NULL);
  }

  // CookieTreeOriginNode methods:
  CookieTreeCookiesNode* GetOrCreateCookiesNode();
  CookieTreeDatabasesNode* GetOrCreateDatabasesNode();
  CookieTreeLocalStoragesNode* GetOrCreateLocalStoragesNode();
  CookieTreeAppCachesNode* GetOrCreateAppCachesNode();

  // Creates an content exception for this origin of type
  // CONTENT_SETTINGS_TYPE_COOKIES.
  void CreateContentException(HostContentSettingsMap* content_settings,
                              ContentSetting setting) const;

  // True if a content exception can be created for this origin.
  bool CanCreateContentException() const;

 private:
  // Pointers to the cookies, databases, local storage and appcache nodes.
  // When we build up the tree we need to quickly get a reference to the COOKIES
  // node to add children. Checking each child and interrogating them to see if
  // they are a COOKIES, APPCACHES, DATABASES etc node seems less preferable
  // than storing an extra pointer per origin.
  CookieTreeCookiesNode* cookies_child_;
  CookieTreeDatabasesNode* databases_child_;
  CookieTreeLocalStoragesNode* local_storages_child_;
  CookieTreeAppCachesNode* appcaches_child_;

  // The URL for which this node was initially created.
  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeOriginNode);
};

// CookieTreeCookieNode ------------------------------------------------------
class CookieTreeCookieNode : public CookieTreeNode {
 public:
  friend class CookieTreeCookiesNode;

  // Does not take ownership of cookie, and cookie should remain valid at least
  // as long as the CookieTreeCookieNode is valid.
  explicit CookieTreeCookieNode(net::CookieMonster::CanonicalCookie* cookie);
  virtual ~CookieTreeCookieNode() {}

  // CookieTreeNode methods:
  virtual void DeleteStoredObjects();
  virtual DetailedInfo GetDetailedInfo() const {
    return DetailedInfo(GetParent()->GetParent()->GetTitle(),
                        DetailedInfo::TYPE_COOKIE, cookie_, NULL, NULL, NULL);
  }

 private:
  // Cookie_ is not owned by the node, and is expected to remain valid as long
  // as the CookieTreeCookieNode is valid.
  net::CookieMonster::CanonicalCookie* cookie_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeCookieNode);
};

class CookieTreeCookiesNode : public CookieTreeNode {
 public:
  CookieTreeCookiesNode();
  virtual ~CookieTreeCookiesNode() {}

  virtual DetailedInfo GetDetailedInfo() const {
    return DetailedInfo(GetParent()->GetTitle(), DetailedInfo::TYPE_COOKIES,
                        NULL, NULL, NULL, NULL);
  }

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

  // Does not take ownership of appcache_info, and appcache_info should remain
  // valid at least as long as the CookieTreeAppCacheNode is valid.
  explicit CookieTreeAppCacheNode(
      const appcache::AppCacheInfo* appcache_info);
  virtual ~CookieTreeAppCacheNode() {}

  virtual void DeleteStoredObjects();
  virtual DetailedInfo GetDetailedInfo() const {
    return DetailedInfo(GetParent()->GetParent()->GetTitle(),
                        DetailedInfo::TYPE_APPCACHE,
                        NULL, NULL, NULL, appcache_info_);
  }

 private:
  const appcache::AppCacheInfo* appcache_info_;
  DISALLOW_COPY_AND_ASSIGN(CookieTreeAppCacheNode);
};

class CookieTreeAppCachesNode : public CookieTreeNode {
 public:
  CookieTreeAppCachesNode();
  virtual ~CookieTreeAppCachesNode() {}

  virtual DetailedInfo GetDetailedInfo() const {
    return DetailedInfo(GetParent()->GetTitle(),
                        DetailedInfo::TYPE_APPCACHES,
                        NULL, NULL, NULL, NULL);
  }

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

  // Does not take ownership of database_info, and database_info should remain
  // valid at least as long as the CookieTreeDatabaseNode is valid.
  explicit CookieTreeDatabaseNode(
      BrowsingDataDatabaseHelper::DatabaseInfo* database_info);
  virtual ~CookieTreeDatabaseNode() {}

  virtual void DeleteStoredObjects();
  virtual DetailedInfo GetDetailedInfo() const {
    return DetailedInfo(GetParent()->GetParent()->GetTitle(),
                        DetailedInfo::TYPE_DATABASE, NULL, database_info_,
                        NULL, NULL);
  }

 private:
  // database_info_ is not owned by the node, and is expected to remain
  // valid as long as the CookieTreeDatabaseNode is valid.
  BrowsingDataDatabaseHelper::DatabaseInfo* database_info_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeDatabaseNode);
};

class CookieTreeDatabasesNode : public CookieTreeNode {
 public:
  CookieTreeDatabasesNode();
  virtual ~CookieTreeDatabasesNode() {}

  virtual DetailedInfo GetDetailedInfo() const {
    return DetailedInfo(GetParent()->GetTitle(),
                        DetailedInfo::TYPE_DATABASES,
                        NULL, NULL, NULL, NULL);
  }

  void AddDatabaseNode(CookieTreeDatabaseNode* child) {
    AddChildSortedByTitle(child);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieTreeDatabasesNode);
};


// CookieTreeLocalStorageNode -------------------------------------------------
class CookieTreeLocalStorageNode : public CookieTreeNode {
 public:
  friend class CookieTreeLocalStoragesNode;

  // Does not take ownership of local_storage_info, and local_storage_info
  // should remain valid at least as long as the CookieTreeStorageNode is valid.
  explicit CookieTreeLocalStorageNode(
      BrowsingDataLocalStorageHelper::LocalStorageInfo* local_storage_info);
  virtual ~CookieTreeLocalStorageNode() {}

  // CookieTreeStorageNode methods:
  virtual void DeleteStoredObjects();
  virtual DetailedInfo GetDetailedInfo() const {
    return DetailedInfo(GetParent()->GetParent()->GetTitle(),
                        DetailedInfo::TYPE_LOCAL_STORAGE, NULL, NULL,
                        local_storage_info_, NULL);
  }

 private:
  // local_storage_info_ is not owned by the node, and is expected to remain
  // valid as long as the CookieTreeLocalStorageNode is valid.
  BrowsingDataLocalStorageHelper::LocalStorageInfo* local_storage_info_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeLocalStorageNode);
};

class CookieTreeLocalStoragesNode : public CookieTreeNode {
 public:
  CookieTreeLocalStoragesNode();
  virtual ~CookieTreeLocalStoragesNode() {}

  virtual DetailedInfo GetDetailedInfo() const {
    return DetailedInfo(GetParent()->GetTitle(),
                        DetailedInfo::TYPE_LOCAL_STORAGES,
                        NULL, NULL, NULL, NULL);
  }

  void AddLocalStorageNode(CookieTreeLocalStorageNode* child) {
    AddChildSortedByTitle(child);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieTreeLocalStoragesNode);
};

class CookiesTreeModel : public TreeNodeModel<CookieTreeNode> {
 public:
  // Because non-cookie nodes are fetched in a background thread, they are not
  // present at the time the Model is created. The Model then notifies its
  // observers for every item added from databases, local storage, and
  // appcache. We extend the Observer interface to add notifications before and
  // after these batch inserts.
  class Observer : public TreeModelObserver {
   public:
    virtual void TreeModelBeginBatch(CookiesTreeModel* model) {}
    virtual void TreeModelEndBatch(CookiesTreeModel* model) {}
  };

  CookiesTreeModel(
      net::CookieMonster* cookie_monster_,
      BrowsingDataDatabaseHelper* database_helper,
      BrowsingDataLocalStorageHelper* local_storage_helper,
      BrowsingDataAppCacheHelper* appcache_helper);
  virtual ~CookiesTreeModel();

  // TreeModel methods:
  // Returns the set of icons for the nodes in the tree. You only need override
  // this if you don't want to use the default folder icons.
  virtual void GetIcons(std::vector<SkBitmap>* icons);

  // Returns the index of the icon to use for |node|. Return -1 to use the
  // default icon. The index is relative to the list of icons returned from
  // GetIcons.
  virtual int GetIconIndex(TreeModelNode* node);

  // CookiesTreeModel methods:
  void DeleteAllStoredObjects();
  void DeleteCookieNode(CookieTreeNode* cookie_node);

  // Filter the origins to only display matched results.
  void UpdateSearchResults(const std::wstring& filter);

  // Overload the Add/Remove observer methods so we can notify about
  // CookiesTreeModel-specific things. Note that this is NOT overriding the
  // method by the same name in TreeNodeModel because the argument type is
  // different. Therefore, if this AddObserver(TreeModelObserver*) is called,
  // the observer will NOT be notified about batching. This is also why we
  // maintain a separate list of observers that are specifically Observer*
  // objects.
  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);

 private:
  enum CookieIconIndex {
    ORIGIN = 0,
    COOKIE = 1,
    DATABASE = 2
  };
  typedef net::CookieMonster::CookieList CookieList;
  typedef std::vector<BrowsingDataDatabaseHelper::DatabaseInfo>
      DatabaseInfoList;
  typedef std::vector<BrowsingDataLocalStorageHelper::LocalStorageInfo>
      LocalStorageInfoList;

  void LoadCookies();
  void LoadCookiesWithFilter(const std::wstring& filter);

  void OnAppCacheModelInfoLoaded();
  void OnDatabaseModelInfoLoaded(const DatabaseInfoList& database_info);
  void OnStorageModelInfoLoaded(const LocalStorageInfoList& local_storage_info);

  void PopulateAppCacheInfoWithFilter(const std::wstring& filter);
  void PopulateDatabaseInfoWithFilter(const std::wstring& filter);
  void PopulateLocalStorageInfoWithFilter(const std::wstring& filter);

  void NotifyObserverBeginBatch();
  void NotifyObserverEndBatch();

  scoped_refptr<net::CookieMonster> cookie_monster_;
  CookieList all_cookies_;

  scoped_refptr<BrowsingDataAppCacheHelper> appcache_helper_;
  scoped_refptr<BrowsingDataDatabaseHelper> database_helper_;
  scoped_refptr<appcache::AppCacheInfoCollection> appcache_info_;
  DatabaseInfoList database_info_list_;

  scoped_refptr<BrowsingDataLocalStorageHelper> local_storage_helper_;
  LocalStorageInfoList local_storage_info_list_;

  // The CookiesTreeModel maintains a separate list of observers that are
  // specifically of the type CookiesTreeModel::Observer.
  ObserverList<Observer> cookies_observer_list_;

  // If this is non-zero, then this model is batching updates (there's a lot of
  // notifications coming down the pipe). This is an integer is used to balance
  // calls to Begin/EndBatch() if they're called in a nested manner.
  int batch_update_;

  friend class CookieTreeAppCacheNode;
  friend class CookieTreeCookieNode;
  friend class CookieTreeDatabaseNode;
  friend class CookieTreeLocalStorageNode;

  DISALLOW_COPY_AND_ASSIGN(CookiesTreeModel);
};

#endif  // CHROME_BROWSER_COOKIES_TREE_MODEL_H_
