// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/intents/intents_model.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/intents/web_intents_registry.h"

IntentsTreeNode::IntentsTreeNode()
  : ui::TreeNode<IntentsTreeNode>(string16()),
    type_(TYPE_ROOT) {}

IntentsTreeNode::IntentsTreeNode(const string16& title)
  : ui::TreeNode<IntentsTreeNode>(title),
    type_(TYPE_ORIGIN) {}

IntentsTreeNode::~IntentsTreeNode() {}

ServiceTreeNode::ServiceTreeNode(const string16& title)
  : IntentsTreeNode(title, IntentsTreeNode::TYPE_SERVICE),
    blocked_(false),
    disabled_(false) {}

ServiceTreeNode::~ServiceTreeNode() {}

IntentsModel::IntentsModel(WebIntentsRegistry* intents_registry)
    : ui::TreeNodeModel<IntentsTreeNode>(new IntentsTreeNode()),
      intents_registry_(intents_registry),
      batch_update_(0) {
  LoadModel();
}

IntentsModel::~IntentsModel() {}

void IntentsModel::AddIntentsTreeObserver(Observer* observer) {
  intents_observer_list_.AddObserver(observer);
  // Call super so that TreeNodeModel can notify, too.
  ui::TreeNodeModel<IntentsTreeNode>::AddObserver(observer);
}

void IntentsModel::RemoveIntentsTreeObserver(Observer* observer) {
  intents_observer_list_.RemoveObserver(observer);
  // Call super so that TreeNodeModel doesn't have dead pointers.
  ui::TreeNodeModel<IntentsTreeNode>::RemoveObserver(observer);
}

string16 IntentsModel::GetTreeNodeId(IntentsTreeNode* node) {
  if (node->Type() == IntentsTreeNode::TYPE_ORIGIN)
    return node->GetTitle();

  // TODO(gbillock): handle TYPE_SERVICE when/if we ever want to do
  // specific managing of them.

  return string16();
}

IntentsTreeNode* IntentsModel::GetTreeNode(std::string path_id) {
  if (path_id.empty())
    return GetRoot();

  std::vector<std::string> node_ids;
  base::SplitString(path_id, ',', &node_ids);

  for (int i = 0; i < GetRoot()->child_count(); ++i) {
    IntentsTreeNode* node = GetRoot()->GetChild(i);
    if (UTF16ToUTF8(node->GetTitle()) == node_ids[0]) {
      if (node_ids.size() == 1)
        return node;
    }
  }

  // TODO: support service nodes?
  return NULL;
}

void IntentsModel::GetChildNodeList(IntentsTreeNode* parent,
                                    int start, int count,
                                    base::ListValue* nodes) {
  for (int i = 0; i < count; ++i) {
    base::DictionaryValue* dict = new base::DictionaryValue;
    IntentsTreeNode* child = parent->GetChild(start + i);
    GetIntentsTreeNodeDictionary(*child, dict);
    nodes->Append(dict);
  }
}

void IntentsModel::GetIntentsTreeNodeDictionary(const IntentsTreeNode& node,
                                                base::DictionaryValue* dict) {
  if (node.Type() == IntentsTreeNode::TYPE_ROOT) {
    return;
  }

  if (node.Type() == IntentsTreeNode::TYPE_ORIGIN) {
    dict->SetString("site", node.GetTitle());
    dict->SetBoolean("hasChildren", node.child_count() > 0);
    return;
  }

  if (node.Type() == IntentsTreeNode::TYPE_SERVICE) {
    const ServiceTreeNode* snode = static_cast<const ServiceTreeNode*>(&node);
    dict->SetString("site", snode->GetTitle());
    dict->SetString("name", snode->ServiceName());
    dict->SetString("url", snode->ServiceUrl());
    dict->SetString("icon", snode->IconUrl());
    dict->SetString("action", snode->Action());
    dict->Set("types", snode->Types().DeepCopy());
    dict->SetBoolean("blocked", snode->IsBlocked());
    dict->SetBoolean("disabled", snode->IsDisabled());
    return;
  }
}

void IntentsModel::LoadModel() {
  NotifyObserverBeginBatch();
  intents_registry_->GetAllIntentProviders(this);
}

void IntentsModel::OnIntentsQueryDone(
    WebIntentsRegistry::QueryID query_id,
    const std::vector<WebIntentData>& intents) {
  for (size_t i = 0; i < intents.size(); ++i) {
    // Eventually do some awesome sorting, grouping, clustering stuff here.
    // For now, just stick it in the model flat.
    IntentsTreeNode* n = new IntentsTreeNode(ASCIIToUTF16(
        intents[i].service_url.host()));
    ServiceTreeNode* ns = new ServiceTreeNode(ASCIIToUTF16(
        intents[i].service_url.host()));
    ns->SetServiceName(intents[i].title);
    ns->SetServiceUrl(ASCIIToUTF16(intents[i].service_url.spec()));
    GURL icon_url = intents[i].service_url.GetOrigin().Resolve("/favicon.ico");
    ns->SetIconUrl(ASCIIToUTF16(icon_url.spec()));
    ns->SetAction(intents[i].action);
    ns->AddType(intents[i].type);
    // Won't generate a notification. OK for now as the next line will.
    n->Add(ns, 0);
    Add(GetRoot(), n, GetRoot()->child_count());
  }

  NotifyObserverEndBatch();
}

void IntentsModel::NotifyObserverBeginBatch() {
  // Only notify the model once if we're batching in a nested manner.
  if (batch_update_++ == 0) {
    FOR_EACH_OBSERVER(Observer,
                      intents_observer_list_,
                      TreeModelBeginBatch(this));
  }
}

void IntentsModel::NotifyObserverEndBatch() {
  // Only notify the observers if this is the outermost call to EndBatch() if
  // called in a nested manner.
  if (--batch_update_ == 0) {
    FOR_EACH_OBSERVER(Observer,
                      intents_observer_list_,
                      TreeModelEndBatch(this));
  }
}
