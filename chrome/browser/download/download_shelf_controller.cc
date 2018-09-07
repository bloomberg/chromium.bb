// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_shelf_controller.h"

#include "chrome/browser/download/offline_item_model_manager.h"
#include "chrome/browser/download/offline_item_model_manager_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/offline_items_collection/core/offline_item.h"

using offline_items_collection::OfflineItemState;

DownloadShelfController::DownloadShelfController(Profile* profile)
    : profile_(profile) {}

DownloadShelfController::~DownloadShelfController() = default;

void DownloadShelfController::OnItemsAdded(
    const OfflineContentProvider::OfflineItemList& items) {
  for (const auto& item : items)
    OnItemUpdated(item);
}

void DownloadShelfController::OnItemRemoved(const ContentId& id) {
  OfflineItemModelManagerFactory::GetForBrowserContext(profile_)
      ->RemoveOfflineItemModel(id);
}

void DownloadShelfController::OnItemUpdated(const OfflineItem& item) {
  if (item.state == OfflineItemState::CANCELLED)
    return;

  OfflineItemModelManager* manager =
      OfflineItemModelManagerFactory::GetForBrowserContext(profile_);
  OfflineItemModel* model = manager->GetOrCreateOfflineItemModel(item);

  if (!model->was_ui_notified()) {
    OnNewOfflineItemReady(item);
    model->set_was_ui_notified(true);
  }
}

void DownloadShelfController::OnNewOfflineItemReady(const OfflineItem& item) {
  Browser* browser = browser = chrome::FindLastActiveWithProfile(profile_);

  if (browser && browser->window()) {
    // Add the offline item to DownloadShelf in the browser window.
  }
}
