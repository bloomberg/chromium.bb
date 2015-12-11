// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_SAVE_CARD_BUBBLE_VIEW_BRIDGE_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_SAVE_CARD_BUBBLE_VIEW_BRIDGE_H_

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "chrome/browser/ui/autofill/save_card_bubble_view.h"
#include "chrome/browser/ui/cocoa/base_bubble_controller.h"

@class BrowserWindowController;
@class SaveCardBubbleViewCocoa;

namespace autofill {

class SaveCardBubbleController;

class SaveCardBubbleViewBridge : public SaveCardBubbleView {
 public:
  SaveCardBubbleViewBridge(SaveCardBubbleController* controller,
                           BrowserWindowController* browser_window_controller);

  // Called by view_controller_ when it is closed.
  void OnViewClosed();

  // SaveCardBubbleView implementation:
  void Hide() override;

 private:
  virtual ~SaveCardBubbleViewBridge();

  // Weak.
  SaveCardBubbleViewCocoa* view_controller_;

  // Weak. Queried for state and notified of user input.
  SaveCardBubbleController* controller_;

  DISALLOW_COPY_AND_ASSIGN(SaveCardBubbleViewBridge);
};

}  // namespace autofill

@interface SaveCardBubbleViewCocoa : BaseBubbleController

// Designated initializer. |bridge| must not be NULL.
- (id)initWithBrowserWindowController:
          (BrowserWindowController*)browserWindowController
                               bridge:
                                   (autofill::SaveCardBubbleViewBridge*)bridge;

@end

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_SAVE_CARD_BUBBLE_VIEW_BRIDGE_H_
