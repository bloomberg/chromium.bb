// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_expanded_state_tracker.h"

#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"

BookmarkExpandedStateTracker::BookmarkExpandedStateTracker(
    Profile* profile,
    const char* path,
    BookmarkModel* bookmark_model)
    : profile_(profile),
      pref_path_(path) {
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
  BookmarkModel* model = profile_->GetBookmarkModel();
  if (!model->IsLoaded())
    return nodes;

  PrefService* prefs = profile_->GetPrefs();
  if (!prefs)
    return nodes;

  const ListValue* value = prefs->GetList(pref_path_);
  if (!value)
    return nodes;

  bool changed = false;
  for (ListValue::const_iterator i = value->begin(); i != value->end(); ++i) {
    std::string value;
    int64 node_id;
    const BookmarkNode* node;
    if ((*i)->GetAsString(&value) && base::StringToInt64(value, &node_id) &&
        (node = model->GetNodeByID(node_id)) != NULL && node->is_folder()) {
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

void BookmarkExpandedStateTracker::UpdatePrefs(const Nodes& nodes) {
  if (!profile_->GetPrefs())
    return;

  ListValue values;
  for (Nodes::const_iterator i = nodes.begin(); i != nodes.end(); ++i) {
    values.Set(values.GetSize(),
               new StringValue(base::Int64ToString((*i)->id())));
  }
  profile_->GetPrefs()->Set(pref_path_, values);
}
