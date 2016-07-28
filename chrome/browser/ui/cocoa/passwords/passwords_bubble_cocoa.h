// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_PASSWORDS_BUBBLE_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_PASSWORDS_BUBBLE_COCOA_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#import "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"

namespace content {
class WebContents;
}

@class ManagePasswordsBubbleController;
@class ManagePasswordsBubbleCocoaNotificationBridge;
class ManagePasswordsIcon;

// Cocoa implementation of the platform-independent password bubble interface.
class ManagePasswordsBubbleCocoa {
 public:
  // Creates and shows the bubble, which owns itself. Does nothing if the bubble
  // is already shown.
  static void Show(content::WebContents* webContents, bool user_action);

  // Closes and deletes the bubble.
  void Close(bool no_animation);
  void Close();

  // Sets the location bar icon that should be updated with state changes.
  void SetIcon(ManagePasswordsIcon* icon) { icon_ = icon; }

  // Accessor for the global bubble.
  static ManagePasswordsBubbleCocoa* instance() { return bubble_; }

 private:
  friend class ManagePasswordsBubbleCocoaTest;
  friend class ManagePasswordsBubbleTest;

  // Instance-specific logic. Clients should use the static interface.
  ManagePasswordsBubbleCocoa(
      content::WebContents* webContents,
      ManagePasswordsBubbleModel::DisplayReason displayReason,
      ManagePasswordsIcon* icon);
  ~ManagePasswordsBubbleCocoa();
  void Show(bool user_action);

  // Cleans up state and deletes itself. Called when the bubble is closed.
  void OnClose();

  ManagePasswordsBubbleModel model_;

  // The location bar icon corresponding to the bubble.
  ManagePasswordsIcon* icon_;

  // The view controller for the bubble. Weak; owns itself. Must be nilled
  // after the bubble is closed.
  ManagePasswordsBubbleController* controller_;

  // WebContents on which the bubble should be displayed. Weak.
  content::WebContents* webContents_;

  // Listens for NSNotificationCenter notifications.
  base::scoped_nsobject<ManagePasswordsBubbleCocoaNotificationBridge> bridge_;

  // The global bubble instance. Deleted by Close().
  static ManagePasswordsBubbleCocoa* bubble_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsBubbleCocoa);
};

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_PASSWORDS_BUBBLE_COCOA_H_
