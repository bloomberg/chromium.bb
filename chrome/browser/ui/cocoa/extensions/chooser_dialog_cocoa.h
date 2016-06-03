// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_CHOOSER_DIALOG_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_CHOOSER_DIALOG_COCOA_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_mac.h"
#include "components/chooser_controller/chooser_controller.h"

namespace content {
class WebContents;
}

@class ChooserDialogCocoaController;

// Displays a chooser dialog as a modal sheet constrained
// to the window/tab displaying the given web contents.
class ChooserDialogCocoa : public ConstrainedWindowMacDelegate,
                           public ChooserController::Observer {
 public:
  ChooserDialogCocoa(content::WebContents* web_contents,
                     ChooserController* chooser_controller);
  ~ChooserDialogCocoa() override;

  // ConstrainedWindowMacDelegate:
  void OnConstrainedWindowClosed(ConstrainedWindowMac* window) override;

  // ChooserController::Observer:
  void OnOptionsInitialized() override;
  void OnOptionAdded(size_t index) override;
  void OnOptionRemoved(size_t index) override;

  // Create and show the modal dialog.
  void ShowDialog();

  // Call this to close the chooser dialog.
  void Dismissed();

  content::WebContents* web_contents() const { return web_contents_; }

 private:
  friend class ChooserDialogCocoaControllerTest;
  base::scoped_nsobject<ChooserDialogCocoaController>
      chooser_dialog_cocoa_controller_;
  std::unique_ptr<ConstrainedWindowMac> constrained_window_;
  content::WebContents* web_contents_;     // Weak.
  ChooserController* chooser_controller_;  // Weak.
};

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_CHOOSER_DIALOG_COCOA_H_
