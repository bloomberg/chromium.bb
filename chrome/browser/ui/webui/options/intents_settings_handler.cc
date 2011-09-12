// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/intents_settings_handler.h"

#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browsing_data_appcache_helper.h"
#include "chrome/browser/browsing_data_database_helper.h"
#include "chrome/browser/browsing_data_file_system_helper.h"
#include "chrome/browser/browsing_data_indexed_db_helper.h"
#include "chrome/browser/browsing_data_local_storage_helper.h"
#include "chrome/browser/intents/web_intents_registry.h"
#include "chrome/browser/intents/web_intents_registry_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "content/browser/webui/web_ui.h"
#include "grit/generated_resources.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/l10n/l10n_util.h"

IntentsSettingsHandler::IntentsSettingsHandler()
    : web_intents_registry_(NULL),
      batch_update_(false) {
}

IntentsSettingsHandler::~IntentsSettingsHandler() {
}

void IntentsSettingsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
    { "intentsDomain", IDS_INTENTS_DOMAIN_COLUMN_HEADER },
    { "intentsServiceData", IDS_INTENTS_SERVICE_DATA_COLUMN_HEADER },
    { "manageIntents", IDS_INTENTS_MANAGE_BUTTON },
    { "removeIntent", IDS_INTENTS_REMOVE_INTENT_BUTTON },
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(localized_strings, "intentsViewPage",
                IDS_INTENTS_MANAGER_WINDOW_TITLE);
}

void IntentsSettingsHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("removeIntent",
      NewCallback(this, &IntentsSettingsHandler::RemoveIntent));
  web_ui_->RegisterMessageCallback("loadIntents",
      NewCallback(this, &IntentsSettingsHandler::LoadChildren));
}

void IntentsSettingsHandler::TreeNodesAdded(ui::TreeModel* model,
                                            ui::TreeModelNode* parent,
                                            int start,
                                            int count) {
  SendChildren(intents_tree_model_->GetRoot());
}

void IntentsSettingsHandler::TreeNodesRemoved(ui::TreeModel* model,
                                              ui::TreeModelNode* parent,
                                              int start,
                                              int count) {
  SendChildren(intents_tree_model_->GetRoot());
}

void IntentsSettingsHandler::TreeModelBeginBatch(IntentsModel* model) {
  batch_update_ = true;
}

void IntentsSettingsHandler::TreeModelEndBatch(IntentsModel* model) {
  batch_update_ = false;

  SendChildren(intents_tree_model_->GetRoot());
}

void IntentsSettingsHandler::EnsureIntentsModelCreated() {
  if (intents_tree_model_.get()) return;

  Profile* profile = Profile::FromWebUI(web_ui_);
  web_data_service_ = profile->GetWebDataService(Profile::EXPLICIT_ACCESS);
  web_intents_registry_ = WebIntentsRegistryFactory::GetForProfile(profile);
  web_intents_registry_->Initialize(web_data_service_.get());
  intents_tree_model_.reset(new IntentsModel(web_intents_registry_));
  intents_tree_model_->AddIntentsTreeObserver(this);
}

void IntentsSettingsHandler::RemoveIntent(const base::ListValue* args) {
  std::string node_path;
  if (!args->GetString(0, &node_path)) {
    return;
  }

  EnsureIntentsModelCreated();

  IntentsTreeNode* node = intents_tree_model_->GetTreeNode(node_path);
  if (node->Type() == IntentsTreeNode::TYPE_ORIGIN) {
    RemoveOrigin(node);
  } else if (node->Type() == IntentsTreeNode::TYPE_SERVICE) {
    ServiceTreeNode* snode = static_cast<ServiceTreeNode*>(node);
    RemoveService(snode);
  }
}

void IntentsSettingsHandler::RemoveOrigin(IntentsTreeNode* node) {
  // TODO(gbillock): This is a known batch update. Worth optimizing?
  while (!node->empty()) {
    IntentsTreeNode* cnode = node->GetChild(0);
    CHECK(cnode->Type() == IntentsTreeNode::TYPE_SERVICE);
    ServiceTreeNode* snode = static_cast<ServiceTreeNode*>(cnode);
    RemoveService(snode);
  }
  delete intents_tree_model_->Remove(node->parent(), node);
}

void IntentsSettingsHandler::RemoveService(ServiceTreeNode* snode) {
  WebIntentData provider;
  provider.service_url = GURL(snode->ServiceUrl());
  provider.action = snode->Action();
  string16 stype;
  if (snode->Types().GetString(0, &stype)) {
    provider.type = stype;  // Really need to iterate here.
  }
  provider.title = snode->ServiceName();
  LOG(INFO) << "Removing service " << snode->ServiceName()
            << " " << snode->ServiceUrl();
  web_intents_registry_->UnregisterIntentProvider(provider);
  delete intents_tree_model_->Remove(snode->parent(), snode);
}

void IntentsSettingsHandler::LoadChildren(const base::ListValue* args) {
  EnsureIntentsModelCreated();

  std::string node_path;
  if (!args->GetString(0, &node_path)) {
    SendChildren(intents_tree_model_->GetRoot());
    return;
  }

  IntentsTreeNode* node = intents_tree_model_->GetTreeNode(node_path);
  SendChildren(node);
}

void IntentsSettingsHandler::SendChildren(IntentsTreeNode* parent) {
  // Early bailout during batch updates. We'll get one after the batch concludes
  // with batch_update_ set false.
  if (batch_update_) return;

  ListValue* children = new ListValue;
  intents_tree_model_->GetChildNodeList(parent, 0, parent->child_count(),
                                        children);

  ListValue args;
  args.Append(parent == intents_tree_model_->GetRoot() ?
      Value::CreateNullValue() :
      Value::CreateStringValue(intents_tree_model_->GetTreeNodeId(parent)));
  args.Append(children);

  web_ui_->CallJavascriptFunction("IntentsView.loadChildren", args);
}
