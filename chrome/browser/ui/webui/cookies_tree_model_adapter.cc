// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cookies_tree_model_adapter.h"

#include "base/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/cookies_tree_model_util.h"
#include "content/browser/webui/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Returns a unique callback name for |adapter|'s requestChildren.
std::string GetRequestChildrenCallbackName(CookiesTreeModelAdapter* adapter) {
  static const char kPrefixLoadCookie[] = "requestChildren";
  return std::string(kPrefixLoadCookie) +
         base::HexEncode(&adapter, sizeof(adapter));
}

}  // namespace

CookiesTreeModelAdapter::CookiesTreeModelAdapter()
    : web_ui_(NULL),
      model_(NULL),
      batch_update_(false) {
}

CookiesTreeModelAdapter::~CookiesTreeModelAdapter() {
  if (model_)
    model_->RemoveCookiesTreeObserver(this);
}

void CookiesTreeModelAdapter::Init(WebUI* web_ui) {
  web_ui_ = web_ui;

  web_ui_->RegisterMessageCallback(GetRequestChildrenCallbackName(this),
      NewCallback(this, &CookiesTreeModelAdapter::RequestChildren));
}

void CookiesTreeModelAdapter::Bind(const std::string& tree_id,
                                   CookiesTreeModel* model) {
  DCHECK(web_ui_);  // We should have been initialized.
  DCHECK(tree_id_.empty() && !model_);  // No existing bindings.

  tree_id_ = tree_id;
  model_ = model;
  model_->AddCookiesTreeObserver(this);

  StringValue tree_id_value(tree_id_);
  StringValue message_name(GetRequestChildrenCallbackName(this));
  web_ui_->CallJavascriptFunction("ui.CookiesTree.setCallback",
      tree_id_value, message_name);

  SendChildren(model_->GetRoot());
}

void CookiesTreeModelAdapter::TreeNodesAdded(ui::TreeModel* model,
                                             ui::TreeModelNode* parent,
                                             int start,
                                             int count) {
  // Skip if there is a batch update in progress.
  if (batch_update_)
    return;

  CookieTreeNode* parent_node = model_->AsNode(parent);

  StringValue tree_id(tree_id_);
  scoped_ptr<Value> parend_id(GetTreeNodeId(parent_node));
  FundamentalValue start_value(start);
  ListValue children;
  cookies_tree_model_util::GetChildNodeList(parent_node, start, count,
                                            &children);
  web_ui_->CallJavascriptFunction("ui.CookiesTree.onTreeItemAdded",
      tree_id, *parend_id.get(), start_value, children);
}

void CookiesTreeModelAdapter::TreeNodesRemoved(ui::TreeModel* model,
                                               ui::TreeModelNode* parent,
                                               int start,
                                               int count) {
  // Skip if there is a batch update in progress.
  if (batch_update_)
    return;

  StringValue tree_id(tree_id_);
  scoped_ptr<Value> parend_id(GetTreeNodeId(model_->AsNode(parent)));
  FundamentalValue start_value(start);
  FundamentalValue count_value(count);
  web_ui_->CallJavascriptFunction("ui.CookiesTree.onTreeItemRemoved",
      tree_id, *parend_id.get(), start_value, count_value);
}

void CookiesTreeModelAdapter::TreeModelBeginBatch(CookiesTreeModel* model) {
  DCHECK(!batch_update_);  // There should be no nested batch begin.
  batch_update_ = true;
}

void CookiesTreeModelAdapter::TreeModelEndBatch(CookiesTreeModel* model) {
  DCHECK(batch_update_);
  batch_update_ = false;

  SendChildren(model_->GetRoot());
}

void CookiesTreeModelAdapter::RequestChildren(const ListValue* args) {
  std::string node_path;
  CHECK(args->GetString(0, &node_path));

  CookieTreeNode* node = cookies_tree_model_util::GetTreeNodeFromPath(
      model_->GetRoot(), node_path);
  if (node)
    SendChildren(node);
}

void CookiesTreeModelAdapter::SendChildren(CookieTreeNode* parent) {
  StringValue tree_id(tree_id_);
  scoped_ptr<Value> parend_id(GetTreeNodeId(model_->AsNode(parent)));
  ListValue children;
  cookies_tree_model_util::GetChildNodeList(parent, 0, parent->child_count(),
      &children);
  web_ui_->CallJavascriptFunction("ui.CookiesTree.setChildren",
      tree_id, *parend_id.get(), children);
}

Value* CookiesTreeModelAdapter::GetTreeNodeId(CookieTreeNode* node) {
  if (node == model_->GetRoot())
    return Value::CreateNullValue();

  return Value::CreateStringValue(
      cookies_tree_model_util::GetTreeNodeId(node));
}
