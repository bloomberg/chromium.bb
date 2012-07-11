// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS2_COOKIES_VIEW_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS2_COOKIES_VIEW_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/cookies_tree_model.h"
#include "chrome/browser/ui/webui/options2/options_ui.h"

class CookiesTreeModelUtil;

namespace options2 {

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

  // Set the context in which this view is used - regular cookies window or
  // the apps cookies window.
  void SetViewContext(const base::ListValue* args);

  // Return the proper callback string, depending on whether the model is
  // in regular cookies mode or apps cookies mode.
  std::string GetCallback(std::string method, CookiesTreeModel* model);

  // Return the proper tree model, depending on the context in which the
  // view operates.
  CookiesTreeModel* GetTreeModel();

  // The Cookies Tree model
  scoped_ptr<CookiesTreeModel> cookies_tree_model_;
  scoped_ptr<CookiesTreeModel> app_cookies_tree_model_;

  // Flag to indicate whether there is a batch update in progress.
  bool batch_update_;

  // A flag to indicate which view is active - the apps dialog or the regular
  // cookies one. This will cause different JavaScript functions to be called.
  bool app_context_;

  scoped_ptr<CookiesTreeModelUtil> model_util_;

  DISALLOW_COPY_AND_ASSIGN(CookiesViewHandler);
};

}  // namespace options2

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS2_COOKIES_VIEW_HANDLER_H_
