// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_INTENTS_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_INTENTS_SETTINGS_HANDLER_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/intents/intents_model.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

class WebDataService;
class WebIntentsRegistry;

// Manage setting up the backing data for the web intents options page.
class IntentsSettingsHandler : public OptionsPageUIHandler,
                               public IntentsModel::Observer {
 public:
  IntentsSettingsHandler();
  virtual ~IntentsSettingsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(base::DictionaryValue* localized_strings);
  virtual void RegisterMessages();

  // IntentsModel::Observer implementation.
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
  virtual void TreeModelBeginBatch(IntentsModel* model) OVERRIDE;
  virtual void TreeModelEndBatch(IntentsModel* model) OVERRIDE;

 private:
  // Creates the IntentsModel if neccessary.
  void EnsureIntentsModelCreated();

  // Updates search filter for cookies tree model.
  void UpdateSearchResults(const base::ListValue* args);

  // Remove all sites data.
  void RemoveAll(const base::ListValue* args);

  // Remove selected sites data.
  void RemoveIntent(const base::ListValue* args);

  // Helper functions for removals.
  void RemoveOrigin(IntentsTreeNode* node);
  void RemoveService(ServiceTreeNode* snode);

  // Trigger for SendChildren to load the JS model.
  void LoadChildren(const base::ListValue* args);

  // Get children nodes data and pass it to 'IntentsView.loadChildren' to
  // update the WebUI.
  void SendChildren(IntentsTreeNode* parent);

  scoped_refptr<WebDataService> web_data_service_;
  WebIntentsRegistry* web_intents_registry_;  // Weak pointer.

  // Backing data model for the intents list.
  scoped_ptr<IntentsModel> intents_tree_model_;

  // Flag to indicate whether there is a batch update in progress.
  bool batch_update_;

  DISALLOW_COPY_AND_ASSIGN(IntentsSettingsHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_INTENTS_SETTINGS_HANDLER_H_
