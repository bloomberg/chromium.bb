// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/intents/web_intents_model.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/intents/default_web_intent_service.h"
#include "chrome/browser/intents/web_intents_registry.h"

WebIntentsTreeNode::WebIntentsTreeNode()
  : ui::TreeNode<WebIntentsTreeNode>(string16()),
    type_(TYPE_ROOT) {}

WebIntentsTreeNode::WebIntentsTreeNode(const string16& title)
  : ui::TreeNode<WebIntentsTreeNode>(title),
    type_(TYPE_ORIGIN) {}

WebIntentsTreeNode::~WebIntentsTreeNode() {}

ServiceTreeNode::ServiceTreeNode(const string16& title)
  : WebIntentsTreeNode(title, WebIntentsTreeNode::TYPE_SERVICE),
    blocked_(false),
    disabled_(false) {}

ServiceTreeNode::~ServiceTreeNode() {}

WebIntentsModel::WebIntentsModel(WebIntentsRegistry* intents_registry)
    : ui::TreeNodeModel<WebIntentsTreeNode>(new WebIntentsTreeNode()),
      intents_registry_(intents_registry),
      batch_update_(0) {
  LoadModel();
}

WebIntentsModel::~WebIntentsModel() {}

void WebIntentsModel::AddWebIntentsTreeObserver(Observer* observer) {
  intents_observer_list_.AddObserver(observer);
  // Call super so that TreeNodeModel can notify, too.
  ui::TreeNodeModel<WebIntentsTreeNode>::AddObserver(observer);
}

void WebIntentsModel::RemoveWebIntentsTreeObserver(Observer* observer) {
  intents_observer_list_.RemoveObserver(observer);
  // Call super so that TreeNodeModel doesn't have dead pointers.
  ui::TreeNodeModel<WebIntentsTreeNode>::RemoveObserver(observer);
}

string16 WebIntentsModel::GetTreeNodeId(WebIntentsTreeNode* node) {
  if (node->Type() == WebIntentsTreeNode::TYPE_ORIGIN)
    return node->GetTitle();

  // TODO(gbillock): handle TYPE_SERVICE when/if we ever want to do
  // specific managing of them.

  return string16();
}

WebIntentsTreeNode* WebIntentsModel::GetTreeNode(std::string path_id) {
  if (path_id.empty())
    return GetRoot();

  std::vector<std::string> node_ids;
  base::SplitString(path_id, ',', &node_ids);

  for (int i = 0; i < GetRoot()->child_count(); ++i) {
    WebIntentsTreeNode* node = GetRoot()->GetChild(i);
    if (UTF16ToUTF8(node->GetTitle()) == node_ids[0]) {
      if (node_ids.size() == 1)
        return node;
    }
  }

  // TODO: support service nodes?
  return NULL;
}

void WebIntentsModel::GetChildNodeList(WebIntentsTreeNode* parent,
                                       int start, int count,
                                       base::ListValue* nodes) {
  for (int i = 0; i < count; ++i) {
    base::DictionaryValue* dict = new base::DictionaryValue;
    WebIntentsTreeNode* child = parent->GetChild(start + i);
    GetWebIntentsTreeNodeDictionary(*child, dict);
    nodes->Append(dict);
  }
}

void WebIntentsModel::GetWebIntentsTreeNodeDictionary(
    const WebIntentsTreeNode& node,
    base::DictionaryValue* dict) {
  if (node.Type() == WebIntentsTreeNode::TYPE_ROOT) {
    dict->SetBoolean("hasChildren", !node.empty());
    dict->SetInteger("numChildren", node.child_count());
    return;
  }

  if (node.Type() == WebIntentsTreeNode::TYPE_ORIGIN) {
    dict->SetString("site", node.GetTitle());
    dict->SetBoolean("hasChildren", !node.empty());
    dict->SetInteger("numChildren", node.child_count());
    return;
  }

  if (node.Type() == WebIntentsTreeNode::TYPE_SERVICE) {
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

WebIntentsTreeNode* WebIntentsModel::GetNodeForHost(const std::string& host) {
  WebIntentsTreeNode* root = GetRoot();
  for (int i = 0; i < root->child_count(); ++i) {
    if (UTF16ToASCII(root->GetChild(i)->GetTitle()) == host)
      return root->GetChild(i);
  }

  // Couldn't find an existing matching node; create a new one.
  WebIntentsTreeNode* origin = new WebIntentsTreeNode(ASCIIToUTF16(host));
  Add(GetRoot(), origin, GetRoot()->child_count());
  return origin;
}

void WebIntentsModel::LoadModel() {
  NotifyObserverBeginBatch();
  intents_registry_->GetAllIntentServices(
      base::Bind(&WebIntentsModel::OnIntentsQueryDone, base::Unretained(this)));
}

void WebIntentsModel::OnIntentsQueryDone(
    const std::vector<webkit_glue::WebIntentServiceData>& services) {
  for (size_t i = 0; i < services.size(); ++i) {
    WebIntentsTreeNode* n = GetNodeForHost(services[i].service_url.host());
    ServiceTreeNode* ns = new ServiceTreeNode(ASCIIToUTF16(
        services[i].service_url.host()));
    ns->SetServiceName(services[i].title);
    ns->SetServiceUrl(ASCIIToUTF16(services[i].service_url.spec()));
    GURL icon_url = services[i].service_url.GetOrigin().Resolve("/favicon.ico");
    ns->SetIconUrl(ASCIIToUTF16(icon_url.spec()));
    ns->SetAction(services[i].action);
    ns->AddType(services[i].type);
    Add(n, ns, n->child_count());
  }

  NotifyObserverEndBatch();
}

void WebIntentsModel::NotifyObserverBeginBatch() {
  // Only notify the model once if we're batching in a nested manner.
  if (batch_update_++ == 0) {
    FOR_EACH_OBSERVER(Observer,
                      intents_observer_list_,
                      TreeModelBeginBatch(this));
  }
}

void WebIntentsModel::NotifyObserverEndBatch() {
  // Only notify the observers if this is the outermost call to EndBatch() if
  // called in a nested manner.
  if (--batch_update_ == 0) {
    FOR_EACH_OBSERVER(Observer,
                      intents_observer_list_,
                      TreeModelEndBatch(this));
  }
}
