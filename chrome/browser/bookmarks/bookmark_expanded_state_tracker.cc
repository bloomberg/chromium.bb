// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_expanded_state_tracker.h"

#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/common/pref_names.h"

BookmarkExpandedStateTracker::BookmarkExpandedStateTracker(
    BookmarkModel* bookmark_model,
    PrefService* pref_service)
    : bookmark_model_(bookmark_model),
      pref_service_(pref_service) {
  bookmark_model->AddObserver(this);
}

BookmarkExpandedStateTracker::~BookmarkExpandedStateTracker() {
}

void BookmarkExpandedStateTracker::SetExpandedNodes(const Nodes& nodes) {
  UpdatePrefs(nodes);
}

BookmarkExpandedStateTracker::Nodes
BookmarkExpandedStateTracker::GetExpandedNodes() {
  Nodes nodes;
  if (!bookmark_model_->loaded())
    return nodes;

  if (!pref_service_)
    return nodes;

  const ListValue* value =
      pref_service_->GetList(prefs::kBookmarkEditorExpandedNodes);
  if (!value)
    return nodes;

  bool changed = false;
  for (ListValue::const_iterator i = value->begin(); i != value->end(); ++i) {
    std::string value;
    int64 node_id;
    const BookmarkNode* node;
    if ((*i)->GetAsString(&value) && base::StringToInt64(value, &node_id) &&
        (node = bookmark_model_->GetNodeByID(node_id)) != NULL &&
        node->is_folder()) {
      nodes.insert(node);
    } else {
      changed = true;
    }
  }
  if (changed)
    UpdatePrefs(nodes);
  return nodes;
}

void BookmarkExpandedStateTracker::Loaded(BookmarkModel* model,
                                          bool ids_reassigned) {
  if (ids_reassigned) {
    // If the ids change we can't trust the value in preferences and need to
    // reset it.
    SetExpandedNodes(Nodes());
  }
}

void BookmarkExpandedStateTracker::BookmarkModelChanged() {
}

void BookmarkExpandedStateTracker::BookmarkModelBeingDeleted(
    BookmarkModel* model) {
  model->RemoveObserver(this);
}

void BookmarkExpandedStateTracker::BookmarkNodeRemoved(
    BookmarkModel* model,
    const BookmarkNode* parent,
    int old_index,
    const BookmarkNode* node) {
  if (!node->is_folder())
    return;  // Only care about folders.

  // Ask for the nodes again, which removes any nodes that were deleted.
  GetExpandedNodes();
}

void BookmarkExpandedStateTracker::BookmarkAllNodesRemoved(
    BookmarkModel* model) {
  // Ask for the nodes again, which removes any nodes that were deleted.
  GetExpandedNodes();
}

void BookmarkExpandedStateTracker::UpdatePrefs(const Nodes& nodes) {
  if (!pref_service_)
    return;

  ListValue values;
  for (Nodes::const_iterator i = nodes.begin(); i != nodes.end(); ++i) {
    values.Set(values.GetSize(),
               new StringValue(base::Int64ToString((*i)->id())));
  }

  pref_service_->Set(prefs::kBookmarkEditorExpandedNodes, values);
}
