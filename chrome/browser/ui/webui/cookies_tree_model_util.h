// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_COOKIES_TREE_MODEL_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_COOKIES_TREE_MODEL_UTIL_H_
#pragma once

#include <string>

class CookieTreeNode;
class DictionaryValue;
class ListValue;

namespace cookies_tree_model_util {

// Returns tree node id. Currently use hex string of node pointer as id.
std::string GetTreeNodeId(CookieTreeNode* node);

// Populate given |dict| with cookie tree node properties.
void GetCookieTreeNodeDictionary(const CookieTreeNode& node,
                                 DictionaryValue* dict);

// Append the children nodes of |parent| in specified range to |nodes| list.
void GetChildNodeList(CookieTreeNode* parent, int start, int count,
                      ListValue* nodes);

// Gets tree node from |path| under |root|. Return NULL if |path| is not valid.
CookieTreeNode* GetTreeNodeFromPath(CookieTreeNode* root,
                                    const std::string& path);

}  // namespace cookies_tree_model_util

#endif  // CHROME_BROWSER_UI_WEBUI_COOKIES_TREE_MODEL_UTIL_H_
