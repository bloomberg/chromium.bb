// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_COOKIES_TREE_MODEL_ADAPTER_H_
#define CHROME_BROWSER_UI_WEBUI_COOKIES_TREE_MODEL_ADAPTER_H_
#pragma once

#include "chrome/browser/cookies_tree_model.h"

class ListValue;
class Value;
class WebUI;

// CookiesTreeModelAdapter binds a CookiesTreeModel with a JS tree. It observes
// tree model changes and forwards them to JS tree. It also provides a
// a callback for JS tree to load children of a specific node.
class CookiesTreeModelAdapter : public CookiesTreeModel::Observer {
 public:
  CookiesTreeModelAdapter();
  virtual ~CookiesTreeModelAdapter();

  // Initializes with given WebUI.
  void Init(WebUI* web_ui);

  // Sets up the bindings between js tree and |model|.
  // Note that this class does not take ownership of the model.
  void Bind(const std::string& tree_id, CookiesTreeModel* model);

 private:
  // CookiesTreeModel::Observer implementation.
  virtual void TreeNodesAdded(ui::TreeModel* model,
                              ui::TreeModelNode* parent,
                              int start,
                              int count);
  virtual void TreeNodesRemoved(ui::TreeModel* model,
                                ui::TreeModelNode* parent,
                                int start,
                                int count);
  virtual void TreeNodeChanged(ui::TreeModel* model, ui::TreeModelNode* node) {}
  virtual void TreeModelBeginBatch(CookiesTreeModel* model);
  virtual void TreeModelEndBatch(CookiesTreeModel* model);

  // JS callback that gets the tree node using the tree path info in |args| and
  // call SendChildren to pass back children nodes data to WebUI.
  void RequestChildren(const ListValue* args);

  // Get children nodes data and pass it to 'CookiesTree.loadChildren' to
  // update the WebUI.
  void SendChildren(CookieTreeNode* parent);

  // Helper function to get a Value* representing id of |node|.
  // Caller needs to free the returned Value.
  Value* GetTreeNodeId(CookieTreeNode* node);

  // Hosting WebUI of the js tree.
  WebUI* web_ui_;

  // Id of JS tree that is managed by this handler.
  std::string tree_id_;

  // The Cookies Tree model. Note that we are not owning the model.
  CookiesTreeModel* model_;

  // Flag to indicate whether there is a batch update in progress.
  bool batch_update_;

  DISALLOW_COPY_AND_ASSIGN(CookiesTreeModelAdapter);
};

#endif  // CHROME_BROWSER_UI_WEBUI_COOKIES_TREE_MODEL_ADAPTER_H_
