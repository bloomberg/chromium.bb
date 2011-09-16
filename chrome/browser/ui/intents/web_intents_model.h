// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INTENTS_WEB_INTENTS_MODEL_H_
#define CHROME_BROWSER_UI_INTENTS_WEB_INTENTS_MODEL_H_
#pragma once

#include "base/values.h"
#include "chrome/browser/intents/web_intents_registry.h"
#include "ui/base/models/tree_node_model.h"

class WebIntentsRegistry;

// The tree structure is a TYPE_ROOT node with title="",
// children are TYPE_ORIGIN nodes with title=origin, whose
// children are TYPE_SERVICE nodes with title=origin, and
// will be of type ServiceTreeNode with data on individual
// services.
class WebIntentsTreeNode : public ui::TreeNode<WebIntentsTreeNode> {
 public:
  WebIntentsTreeNode();
  explicit WebIntentsTreeNode(const string16& title);

  virtual ~WebIntentsTreeNode();

  enum NodeType {
    TYPE_ROOT,
    TYPE_ORIGIN,
    TYPE_SERVICE,
  };

  NodeType Type() const { return type_; }

 protected:
  WebIntentsTreeNode(const string16& title, NodeType type)
      : ui::TreeNode<WebIntentsTreeNode>(title),
        type_(type) {}

 private:
  NodeType type_;
};

// Tree node representing particular services presented by an origin.
class ServiceTreeNode : public WebIntentsTreeNode {
 public:
  explicit ServiceTreeNode(const string16& title);
  virtual ~ServiceTreeNode();

  const string16& ServiceName() const { return service_name_; }
  const string16& ServiceUrl() const { return service_url_; }
  const string16& IconUrl() const { return icon_url_; }
  const string16& Action() const { return action_; }
  const base::ListValue& Types() const { return types_; }
  bool IsBlocked() const { return blocked_; }
  bool IsDisabled() const { return disabled_; }

  void SetServiceName(string16 name) { service_name_ = name; }
  void SetServiceUrl(string16 url) { service_url_ = url; }
  void SetIconUrl(string16 url) { icon_url_ = url; }
  void SetAction(string16 action) { action_ = action; }
  void AddType(string16 type) { types_.Append(Value::CreateStringValue(type)); }
  void SetBlocked(bool blocked) { blocked_ = blocked; }
  void SetDisabled(bool disabled) { disabled_ = disabled; }

 private:
  string16 service_name_;
  string16 icon_url_;
  string16 service_url_;
  string16 action_;
  base::ListValue types_;

  // TODO(gbillock): these are kind of a placeholder for exceptions data.
  bool blocked_;
  bool disabled_;
};

// UI-backing tree model of the data in the WebIntentsRegistry.
class WebIntentsModel : public ui::TreeNodeModel<WebIntentsTreeNode>,
                        public WebIntentsRegistry::Consumer {
 public:
  // Because nodes are fetched in a background thread, they are not
  // present at the time the Model is created. The Model then notifies its
  // observers for every item added.
  class Observer : public ui::TreeModelObserver {
   public:
    virtual void TreeModelBeginBatch(WebIntentsModel* model) {}
    virtual void TreeModelEndBatch(WebIntentsModel* model) {}
  };

  explicit WebIntentsModel(WebIntentsRegistry* intents_registry);
  virtual ~WebIntentsModel();

  void AddWebIntentsTreeObserver(Observer* observer);
  void RemoveWebIntentsTreeObserver(Observer* observer);

  string16 GetTreeNodeId(WebIntentsTreeNode* node);
  WebIntentsTreeNode* GetTreeNode(std::string path_id);
  void GetChildNodeList(WebIntentsTreeNode* parent, int start, int count,
                        base::ListValue* nodes);
  void GetWebIntentsTreeNodeDictionary(const WebIntentsTreeNode& node,
                                       base::DictionaryValue* dict);

  virtual void OnIntentsQueryDone(
      WebIntentsRegistry::QueryID query_id,
      const std::vector<WebIntentData>& intents) OVERRIDE;

 private:
  // Loads the data model from the WebIntentsRegistry.
  // TODO(gbillock): need an observer on that to absorb async updates?
  void LoadModel();

  // Do batch-specific notifies for updates coming from the LoadModel.
  void NotifyObserverBeginBatch();
  void NotifyObserverEndBatch();

  // The backing registry. Weak pointer.
  WebIntentsRegistry* intents_registry_;

  // Separate list of observers that'll get batch updates.
  ObserverList<Observer> intents_observer_list_;

  // Batch update nesting level. Incremented to indicate that we're in
  // the middle of a batch update.
  int batch_update_;
};

#endif  // CHROME_BROWSER_UI_INTENTS_WEB_INTENTS_MODEL_H_
