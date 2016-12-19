// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/tools_menu_button_observer_bridge.h"

#include "components/reading_list/ios/reading_list_model.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_tools_menu_button.h"

ToolsMenuButtonObserverBridge::ToolsMenuButtonObserverBridge(
    ReadingListModel* readingListModel,
    ToolbarToolsMenuButton* button)
    : readingListModel_(readingListModel), button_([button retain]) {
  readingListModel_->AddObserver(this);
};

ToolsMenuButtonObserverBridge::~ToolsMenuButtonObserverBridge() {
  readingListModel_->RemoveObserver(this);
}

void ToolsMenuButtonObserverBridge::ReadingListModelLoaded(
    const ReadingListModel* model) {
  UpdateButtonWithModel(model);
}
void ToolsMenuButtonObserverBridge::ReadingListDidApplyChanges(
    ReadingListModel* model) {
  UpdateButtonWithModel(model);
}

void ToolsMenuButtonObserverBridge::UpdateButtonWithModel(
    const ReadingListModel* model) {
  bool readingListContainsUnreadItems = model->unread_size() > 0;
  [button_ setReadingListContainsUnreadItems:readingListContainsUnreadItems];
}
