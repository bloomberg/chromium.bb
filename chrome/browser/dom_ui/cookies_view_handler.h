// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_COOKIES_VIEW_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_COOKIES_VIEW_HANDLER_H_
#pragma once

#include "base/scoped_ptr.h"
#include "chrome/browser/cookies_tree_model.h"
#include "chrome/browser/dom_ui/options_ui.h"

class CookiesViewHandler : public OptionsPageUIHandler,
                           public CookiesTreeModel::Observer {
 public:
  CookiesViewHandler();
  virtual ~CookiesViewHandler();

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);
  virtual void Initialize();
  virtual void RegisterMessages();

  // TreeModel::Observer implementation.
  virtual void TreeNodesAdded(TreeModel* model,
                              TreeModelNode* parent,
                              int start,
                              int count);
  virtual void TreeNodesRemoved(TreeModel* model,
                                TreeModelNode* parent,
                                int start,
                                int count);
  virtual void TreeNodeChanged(TreeModel* model, TreeModelNode* node) {}

 private:
  // Updates search filter for cookies tree model.
  void UpdateSearchResults(const ListValue* args);

  // Remove all sites data.
  void RemoveAll(const ListValue* args);

  // Remove selected sites data.
  void Remove(const ListValue* args);

  // The Cookies Tree model
  scoped_ptr<CookiesTreeModel> cookies_tree_model_;

  DISALLOW_COPY_AND_ASSIGN(CookiesViewHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_COOKIES_VIEW_HANDLER_H_
