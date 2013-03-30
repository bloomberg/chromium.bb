// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_bridge.h"

#include "apps/app_launcher.h"
#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_controller.h"
#include "chrome/common/pref_names.h"


BookmarkBarBridge::BookmarkBarBridge(Profile* profile,
                                     BookmarkBarController* controller,
                                     BookmarkModel* model)
    : controller_(controller),
      model_(model),
      batch_mode_(false) {
  model_->AddObserver(this);

  // Bookmark loading is async; it may may not have happened yet.
  // We will be notified when that happens with the AddObserver() call.
  if (model->IsLoaded())
    Loaded(model, false);

  profile_pref_registrar_.Init(profile->GetPrefs());
  profile_pref_registrar_.Add(
      prefs::kShowAppsShortcutInBookmarkBar,
      base::Bind(&BookmarkBarBridge::OnAppsPageShortcutVisibilityPrefChanged,
                 base::Unretained(this)));

  // The first check for the app launcher is asynchronous, run it now.
  apps::GetIsAppLauncherEnabled(
      base::Bind(&BookmarkBarBridge::OnAppLauncherEnabledCompleted,
                 base::Unretained(this)));
}

BookmarkBarBridge::~BookmarkBarBridge() {
  model_->RemoveObserver(this);
}

void BookmarkBarBridge::Loaded(BookmarkModel* model, bool ids_reassigned) {
  [controller_ loaded:model];
}

void BookmarkBarBridge::BookmarkModelBeingDeleted(BookmarkModel* model) {
  [controller_ beingDeleted:model];
}

void BookmarkBarBridge::BookmarkNodeMoved(BookmarkModel* model,
                                          const BookmarkNode* old_parent,
                                          int old_index,
                                          const BookmarkNode* new_parent,
                                          int new_index) {
  [controller_ nodeMoved:model
               oldParent:old_parent oldIndex:old_index
               newParent:new_parent newIndex:new_index];
}

void BookmarkBarBridge::BookmarkNodeAdded(BookmarkModel* model,
                                          const BookmarkNode* parent,
                                          int index) {
  if (!batch_mode_) {
    [controller_ nodeAdded:model parent:parent index:index];
  }
}

void BookmarkBarBridge::BookmarkNodeRemoved(BookmarkModel* model,
                                            const BookmarkNode* parent,
                                            int old_index,
                                            const BookmarkNode* node) {
  [controller_ nodeRemoved:model parent:parent index:old_index];
}

void BookmarkBarBridge::BookmarkAllNodesRemoved(BookmarkModel* model) {
  [controller_ loaded:model];
}

void BookmarkBarBridge::BookmarkNodeChanged(BookmarkModel* model,
                                            const BookmarkNode* node) {
  [controller_ nodeChanged:model node:node];
}

void BookmarkBarBridge::BookmarkNodeFaviconChanged(BookmarkModel* model,
                                                   const BookmarkNode* node) {
  [controller_ nodeFaviconLoaded:model node:node];
}

void BookmarkBarBridge::BookmarkNodeChildrenReordered(
    BookmarkModel* model, const BookmarkNode* node) {
  [controller_ nodeChildrenReordered:model node:node];
}

void BookmarkBarBridge::ExtensiveBookmarkChangesBeginning(
    BookmarkModel* model) {
  batch_mode_ = true;
}

void BookmarkBarBridge::ExtensiveBookmarkChangesEnded(BookmarkModel* model) {
  batch_mode_ = false;
  [controller_ loaded:model];
}

void BookmarkBarBridge::OnAppsPageShortcutVisibilityPrefChanged() {
  [controller_ updateAppsPageShortcutButtonVisibility];
}

void BookmarkBarBridge::OnAppLauncherEnabledCompleted(
    bool app_launcher_enabled) {
  [controller_ updateAppsPageShortcutButtonVisibility];
}
