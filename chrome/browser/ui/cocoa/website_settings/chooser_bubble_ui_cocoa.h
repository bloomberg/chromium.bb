// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_WEBSITE_SETTINGS_CHOOSER_BUBBLE_UI_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_WEBSITE_SETTINGS_CHOOSER_BUBBLE_UI_COCOA_H_

#import <Cocoa/Cocoa.h>

#include "base/macros.h"
#include "chrome/browser/ui/website_settings/chooser_bubble_controller.h"
#include "components/bubble/bubble_ui.h"

class Browser;
@class ChooserBubbleUiController;

// ChooserBubbleUiCocoa implements a chooser-based permission model.
// It uses |NSTableView| to show a list of options for user to grant
// permission. It can be used by the WebUSB or WebBluetooth APIs.
class ChooserBubbleUiCocoa : public BubbleUi,
                             public ChooserBubbleController::Observer {
 public:
  ChooserBubbleUiCocoa(Browser* browser,
                       ChooserBubbleController* chooser_bubble_controller);
  ~ChooserBubbleUiCocoa() override;

  // BubbleUi:
  void Show(BubbleReference bubble_reference) override;
  void Close() override;
  void UpdateAnchorPosition() override;

  // ChooserBubbleController::Observer:
  void OnOptionsInitialized() override;
  void OnOptionAdded(size_t index) override;
  void OnOptionRemoved(size_t index) override;

  // Called when |chooser_bubble_ui_controller_| is closing.
  void OnBubbleClosing();

 private:
  Browser* browser_;                                    // Weak.
  ChooserBubbleController* chooser_bubble_controller_;  // Weak.
  // Cocoa-side chooser bubble UI controller. Weak, as it will close itself.
  ChooserBubbleUiController* chooser_bubble_ui_controller_;

  DISALLOW_COPY_AND_ASSIGN(ChooserBubbleUiCocoa);
};

#endif  // CHROME_BROWSER_UI_COCOA_WEBSITE_SETTINGS_CHOOSER_BUBBLE_UI_COCOA_H_
