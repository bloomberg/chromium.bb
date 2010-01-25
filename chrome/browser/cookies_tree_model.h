// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COOKIES_TREE_MODEL_H_
#define CHROME_BROWSER_COOKIES_TREE_MODEL_H_

#include <string>
#include <vector>

#include "app/tree_node_model.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/browsing_data_local_storage_helper.h"
#include "net/base/cookie_monster.h"

class CookiesTreeModel;
class CookieTreeLocalStorageNode;
class CookieTreeLocalStoragesNode;
class CookieTreeCookieNode;
class CookieTreeCookiesNode;
class CookieTreeOriginNode;
class Profile;

// CookieTreeNode -------------------------------------------------------------
// The base node type in the Cookies + Local Storage options view, from which
// all other types are derived. Specialized from TreeNode in that it has a
// notion of deleting objects stored in the profile, and being able to have
// its children do the same.
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
      TYPE_LOCAL_STORAGES,  // This is used for CookieTreeLocalStoragesNode.
      TYPE_LOCAL_STORAGE,  // This is used for CookieTreeLocalStorageNode.
    };

    DetailedInfo(const std::wstring& origin, NodeType node_type,
        const net::CookieMonster::CookieListPair* cookie,
        const BrowsingDataLocalStorageHelper::LocalStorageInfo*
            local_storage_info)
        : origin(origin),
          node_type(node_type),
          cookie(cookie),
          local_storage_info(local_storage_info) {
      if (node_type == TYPE_LOCAL_STORAGE)
        DCHECK(local_storage_info);
    }

    std::wstring origin;
    NodeType node_type;
    const net::CookieMonster::CookieListPair* cookie;
    const BrowsingDataLocalStorageHelper::LocalStorageInfo* local_storage_info;
  };

  CookieTreeNode() {}
  explicit CookieTreeNode(const std::wstring& title)
      : TreeNode<CookieTreeNode>(title) {}

  // Delete backend storage for this node, and any children nodes. (E.g. delete
  // the cookie from CookieMonster, clear the database, and so forth.)
  virtual void DeleteStoredObjects();

  // Gets a pointer back to the associated model for the tree we are in.
  virtual CookiesTreeModel* GetModel() const;

  // Returns a struct with detailed information used to populate the details
  // part of the view.
  virtual DetailedInfo GetDetailedInfo() const = 0;

 private:

  DISALLOW_COPY_AND_ASSIGN(CookieTreeNode);
};

// CookieTreeRootNode ---------------------------------------------------------
// The node at the root of the CookieTree that gets inserted into the view.
class CookieTreeRootNode : public CookieTreeNode {
 public:
  explicit CookieTreeRootNode(CookiesTreeModel* model) : model_(model) {}
  virtual ~CookieTreeRootNode() {}

  CookieTreeOriginNode* GetOrCreateOriginNode(const std::wstring& origin);

  // CookieTreeNode methods:
  virtual CookiesTreeModel* GetModel() const { return model_; }
  virtual DetailedInfo GetDetailedInfo() const {
    return DetailedInfo(std::wstring(), DetailedInfo::TYPE_ROOT, NULL, NULL);
  }
 private:

  CookiesTreeModel* model_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeRootNode);
};

// CookieTreeOriginNode -------------------------------------------------------
class CookieTreeOriginNode : public CookieTreeNode {
 public:
  explicit CookieTreeOriginNode(const std::wstring& origin)
      : CookieTreeNode(origin), cookies_child_(NULL),
        local_storages_child_(NULL) {}
  virtual ~CookieTreeOriginNode() {}

  // CookieTreeNode methods:
  virtual DetailedInfo GetDetailedInfo() const {
    return DetailedInfo(GetTitle(), DetailedInfo::TYPE_ORIGIN, NULL, NULL);
  }

  // CookieTreeOriginNode methods:
  CookieTreeCookiesNode* GetOrCreateCookiesNode();
  CookieTreeLocalStoragesNode* GetOrCreateLocalStoragesNode();

 private:

  // A pointer to the COOKIES node. Eventually we will also have database,
  // appcache, local storage, ..., and when we build up the tree we need to
  // quickly get a reference to the COOKIES node to add children. Checking each
  // child and interrogating them to see if they are a COOKIES, APPCACHES,
  // DATABASES etc node seems less preferable than storing an extra pointer per
  // origin.
  CookieTreeCookiesNode* cookies_child_;
  CookieTreeLocalStoragesNode* local_storages_child_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeOriginNode);
};

// CookieTreeCookiesNode ------------------------------------------------------
class CookieTreeCookiesNode : public CookieTreeNode {
 public:
  CookieTreeCookiesNode();
  virtual ~CookieTreeCookiesNode() {}

  // CookieTreeNode methods:
  virtual DetailedInfo GetDetailedInfo() const {
    return DetailedInfo(GetParent()->GetTitle(), DetailedInfo::TYPE_COOKIES,
                        NULL, NULL);
  }

  // CookieTreeCookiesNode methods:
  void AddCookieNode(CookieTreeCookieNode* child);

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieTreeCookiesNode);
};

class CookieTreeCookieNode : public CookieTreeNode {
 public:
  friend class CookieTreeCookiesNode;

  // Does not take ownership of cookie, and cookie should remain valid at least
  // as long as the CookieTreeCookieNode is valid.
  explicit CookieTreeCookieNode(net::CookieMonster::CookieListPair* cookie);
  virtual ~CookieTreeCookieNode() {}

  // CookieTreeNode methods:
  virtual void DeleteStoredObjects();
  virtual DetailedInfo GetDetailedInfo() const {
    return DetailedInfo(GetParent()->GetParent()->GetTitle(),
                        DetailedInfo::TYPE_COOKIE, cookie_, NULL);
  }

 private:
  // Comparator functor, takes CookieTreeNode so that we can use it in
  // lower_bound using children()'s iterators, which are CookieTreeNode*.
  class CookieNodeComparator {
   public:
    bool operator() (const CookieTreeNode* lhs, const CookieTreeNode* rhs);
  };

  // Cookie_ is not owned by the node, and is expected to remain valid as long
  // as the CookieTreeCookieNode is valid.
  net::CookieMonster::CookieListPair* cookie_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeCookieNode);
};

// CookieTreeLocalStoragesNode -------------------------------------------------
class CookieTreeLocalStoragesNode : public CookieTreeNode {
 public:
  CookieTreeLocalStoragesNode();
  virtual ~CookieTreeLocalStoragesNode() {}

  // CookieTreeNode methods:
  virtual DetailedInfo GetDetailedInfo() const {
    return DetailedInfo(GetParent()->GetTitle(),
                        DetailedInfo::TYPE_LOCAL_STORAGES,
                        NULL,
                        NULL);
  }

  // CookieTreeStoragesNode methods:
  void AddLocalStorageNode(CookieTreeLocalStorageNode* child);

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieTreeLocalStoragesNode);
};

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
                        DetailedInfo::TYPE_LOCAL_STORAGE, NULL,
                        local_storage_info_);
  }

 private:
  // Comparator functor, takes CookieTreeNode so that we can use it in
  // lower_bound using children()'s iterators, which are CookieTreeNode*.
  class CookieNodeComparator {
   public:
    bool operator() (const CookieTreeNode* lhs, const CookieTreeNode* rhs);
  };

  // local_storage_info_ is not owned by the node, and is expected to remain
  // valid as long as the CookieTreeLocalStorageNode is valid.
  BrowsingDataLocalStorageHelper::LocalStorageInfo* local_storage_info_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeLocalStorageNode);
};

class CookiesTreeModel : public TreeNodeModel<CookieTreeNode> {
 public:
  CookiesTreeModel(
      Profile* profile,
      BrowsingDataLocalStorageHelper* browsing_data_local_storage_helper);
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
  void DeleteCookie(const net::CookieMonster::CookieListPair& cookie);
  void DeleteAllCookies();
  void DeleteCookieNode(CookieTreeNode* cookie_node);
  void DeleteLocalStorage(const FilePath& file_path);
  void DeleteAllLocalStorage();

  // Filter the origins to only display matched results.
  void UpdateSearchResults(const std::wstring& filter);

 private:
  enum CookieIconIndex {
    ORIGIN = 0,
    COOKIE = 1,
    LOCAL_STORAGE = 2,
  };
  typedef net::CookieMonster::CookieList CookieList;
  typedef std::vector<net::CookieMonster::CookieListPair*> CookiePtrList;
  typedef std::vector<BrowsingDataLocalStorageHelper::LocalStorageInfo>
      LocalStorageInfoList;

  void LoadCookies();
  void LoadCookiesWithFilter(const std::wstring& filter);

  void OnStorageModelInfoLoaded(const LocalStorageInfoList& local_storage_info);

  void PopulateLocalStorageInfoWithFilter(const std::wstring& filter);

  // The profile from which this model sources cookies.
  Profile* profile_;
  CookieList all_cookies_;

  scoped_refptr<BrowsingDataLocalStorageHelper> local_storage_helper_;
  LocalStorageInfoList local_storage_info_list_;

  DISALLOW_COPY_AND_ASSIGN(CookiesTreeModel);
};

#endif  // CHROME_BROWSER_COOKIES_TREE_MODEL_H_
