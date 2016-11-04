// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_COOKIES_VIEW_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_COOKIES_VIEW_HANDLER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

class CookiesTreeModelUtil;

namespace settings {

class CookiesViewHandler : public SettingsPageUIHandler,
                           public CookiesTreeModel::Observer {
 public:
  CookiesViewHandler();
  ~CookiesViewHandler() override;

  // SettingsPageUIHandler:
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;
  void RegisterMessages() override;

  // CookiesTreeModel::Observer:
  void TreeNodesAdded(ui::TreeModel* model,
                      ui::TreeModelNode* parent,
                      int start,
                      int count) override;
  void TreeNodesRemoved(ui::TreeModel* model,
                        ui::TreeModelNode* parent,
                        int start,
                        int count) override;
  void TreeNodeChanged(ui::TreeModel* model, ui::TreeModelNode* node) override {
  }
  void TreeModelBeginBatch(CookiesTreeModel* model) override;
  void TreeModelEndBatch(CookiesTreeModel* model) override;

 private:
  // Creates the CookiesTreeModel if neccessary.
  void EnsureCookiesTreeModelCreated();

  // Updates search filter for cookies tree model.
  void HandleUpdateSearchResults(const base::ListValue* args);

  // Retrieve cookie details for a specific site.
  void HandleGetCookieDetails(const base::ListValue* args);

  // Remove all sites data.
  void HandleRemoveAll(const base::ListValue* args);

  // Remove selected sites data.
  void HandleRemove(const base::ListValue* args);

  // Get the tree node using the tree path info in |args| and call
  // SendChildren to pass back children nodes data to WebUI.
  void HandleLoadChildren(const base::ListValue* args);

  // Get children nodes data and pass it to 'CookiesView.loadChildren' to
  // update the WebUI.
  void SendChildren(const CookieTreeNode* parent);

  // Package and send cookie details for a site.
  void SendCookieDetails(const CookieTreeNode* parent);

  // Reloads the CookiesTreeModel and passes the nodes to
  // 'CookiesView.loadChildren' to update the WebUI.
  void HandleReloadCookies(const base::ListValue* args);

  // The Cookies Tree model
  std::unique_ptr<CookiesTreeModel> cookies_tree_model_;

  // Flag to indicate whether there is a batch update in progress.
  bool batch_update_;

  std::unique_ptr<CookiesTreeModelUtil> model_util_;

  // The callback ID for the current outstanding request.
  std::string callback_id_;

  DISALLOW_COPY_AND_ASSIGN(CookiesViewHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_COOKIES_VIEW_HANDLER_H_
