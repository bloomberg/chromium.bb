// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_COOKIES_TREE_MODEL_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_COOKIES_TREE_MODEL_UTIL_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/id_map.h"

class CookieTreeNode;

namespace base {
class DictionaryValue;
class ListValue;
}

class CookiesTreeModelUtil {
 public:
  CookiesTreeModelUtil();
  ~CookiesTreeModelUtil();

  // Finds or creates an ID for given |node| and returns it as string.
  std::string GetTreeNodeId(const CookieTreeNode* node);

  // Append the children nodes of |parent| in specified range to |nodes| list.
  void GetChildNodeList(const CookieTreeNode* parent,
                        int start,
                        int count,
                        base::ListValue* nodes);

  // Gets tree node from |path| under |root|. |path| is comma separated list of
  // ids. |id_map| translates ids into object pointers. Return NULL if |path|
  // is not valid.
  const CookieTreeNode* GetTreeNodeFromPath(const CookieTreeNode* root,
                                            const std::string& path);

 private:
  typedef IDMap<const CookieTreeNode> CookiesTreeNodeIdMap;
  typedef std::map<const CookieTreeNode*, int32> CookieTreeNodeMap;

  // Populate given |dict| with cookie tree node properties. |id_map| maps
  // a CookieTreeNode to an ID and creates a new ID if |node| is not in the
  // maps. Returns false if the |node| does not need to be shown.
  bool GetCookieTreeNodeDictionary(const CookieTreeNode& node,
                                   base::DictionaryValue* dict);

  // IDMap to create unique ID and look up the object for an ID.
  CookiesTreeNodeIdMap id_map_;

  // Reverse look up map to find the ID for a node.
  CookieTreeNodeMap node_map_;

  DISALLOW_COPY_AND_ASSIGN(CookiesTreeModelUtil);
};

#endif  // CHROME_BROWSER_UI_WEBUI_COOKIES_TREE_MODEL_UTIL_H_
