// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLS_MENU_BUTTON_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLS_MENU_BUTTON_OBSERVER_BRIDGE_H_

#import <UIKit/UIKit.h>

#import "base/mac/scoped_nsobject.h"
#include "components/reading_list/ios/reading_list_model_observer.h"

@class ToolbarToolsMenuButton;

// C++ bridge informing a ToolbarToolsMenuButton whether the Reading List Model
// contains unread items.
class ToolsMenuButtonObserverBridge : public ReadingListModelObserver {
 public:
  // Creates the bridge to the ToolbarToolsMenuButton |button|.
  ToolsMenuButtonObserverBridge(ReadingListModel* readingListModel,
                                ToolbarToolsMenuButton* button);

  ~ToolsMenuButtonObserverBridge() final;

  // ReadingListModelObserver implementation.
  void ReadingListModelLoaded(const ReadingListModel* model) override;
  void ReadingListDidApplyChanges(ReadingListModel* model) override;

 private:
  // Notify the ToolbarToolsMenuButton of whether the reading list contains
  // unread items.
  void UpdateButtonWithModel(const ReadingListModel* model);
  ReadingListModel* readingListModel_;
  base::scoped_nsobject<ToolbarToolsMenuButton> button_;
};

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLS_MENU_BUTTON_OBSERVER_BRIDGE_H_
