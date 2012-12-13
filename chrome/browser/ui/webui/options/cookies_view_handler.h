// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_COOKIES_VIEW_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_COOKIES_VIEW_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

class CookiesTreeModelUtil;

namespace options {

class CookiesViewHandler : public OptionsPageUIHandler,
                           public CookiesTreeModel::Observer {
 public:
  CookiesViewHandler();
  virtual ~CookiesViewHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

  // CookiesTreeModel::Observer implementation.
  virtual void TreeNodesAdded(ui::TreeModel* model,
                              ui::TreeModelNode* parent,
                              int start,
                              int count) OVERRIDE;
  virtual void TreeNodesRemoved(ui::TreeModel* model,
                                ui::TreeModelNode* parent,
                                int start,
                                int count) OVERRIDE;
  virtual void TreeNodeChanged(ui::TreeModel* model,
                               ui::TreeModelNode* node) OVERRIDE {}
  virtual void TreeModelBeginBatch(CookiesTreeModel* model) OVERRIDE;
  virtual void TreeModelEndBatch(CookiesTreeModel* model) OVERRIDE;

 private:
  // Creates the CookiesTreeModel if neccessary.
  void EnsureCookiesTreeModelCreated();

  // Updates search filter for cookies tree model.
  void UpdateSearchResults(const base::ListValue* args);

  // Remove all sites data.
  void RemoveAll(const base::ListValue* args);

  // Remove selected sites data.
  void Remove(const base::ListValue* args);

  // Get the tree node using the tree path info in |args| and call
  // SendChildren to pass back children nodes data to WebUI.
  void LoadChildren(const base::ListValue* args);

  // Get children nodes data and pass it to 'CookiesView.loadChildren' to
  // update the WebUI.
  void SendChildren(const CookieTreeNode* parent);

  // Reloads the CookiesTreeModel and passes the nodes to
  // 'CookiesView.loadChildren' to update the WebUI.
  void ReloadCookies(const base::ListValue* args);

  // The Cookies Tree model
  scoped_ptr<CookiesTreeModel> cookies_tree_model_;

  // Flag to indicate whether there is a batch update in progress.
  bool batch_update_;

  scoped_ptr<CookiesTreeModelUtil> model_util_;

  DISALLOW_COPY_AND_ASSIGN(CookiesViewHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_COOKIES_VIEW_HANDLER_H_
