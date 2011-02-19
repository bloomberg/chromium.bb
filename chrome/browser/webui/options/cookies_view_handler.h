// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBUI_OPTIONS_COOKIES_VIEW_HANDLER_H_
#define CHROME_BROWSER_WEBUI_OPTIONS_COOKIES_VIEW_HANDLER_H_
#pragma once

#include "base/scoped_ptr.h"
#include "chrome/browser/cookies_tree_model.h"
#include "chrome/browser/dom_ui/options/options_ui.h"

class CookiesViewHandler : public OptionsPageUIHandler,
                           public CookiesTreeModel::Observer {
 public:
  CookiesViewHandler();
  virtual ~CookiesViewHandler();

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);
  virtual void RegisterMessages();

  // ui::TreeModel::Observer implementation.
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

 private:
  // Updates search filter for cookies tree model.
  void UpdateSearchResults(const ListValue* args);

  // Remove all sites data.
  void RemoveAll(const ListValue* args);

  // Remove selected sites data.
  void Remove(const ListValue* args);

  // Get the tree node using the tree path info in |args| and call
  // SendChildren to pass back children nodes data to WebUI.
  void LoadChildren(const ListValue* args);

  // Gets tree node from given path. Return NULL if path is not valid.
  CookieTreeNode* GetTreeNodeFromPath(const std::string& path);

  // Get children nodes data and pass it to 'CookiesView.loadChildren' to
  // update the WebUI.
  void SendChildren(CookieTreeNode* parent);

  // The Cookies Tree model
  scoped_ptr<CookiesTreeModel> cookies_tree_model_;

  // Flag to indicate whether there is a batch update in progress.
  bool batch_update_;

  DISALLOW_COPY_AND_ASSIGN(CookiesViewHandler);
};

#endif  // CHROME_BROWSER_WEBUI_OPTIONS_COOKIES_VIEW_HANDLER_H_
