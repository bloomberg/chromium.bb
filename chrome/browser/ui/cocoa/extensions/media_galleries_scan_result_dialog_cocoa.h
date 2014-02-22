// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_MEDIA_GALLERIES_SCAN_RESULT_DIALOG_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_MEDIA_GALLERIES_SCAN_RESULT_DIALOG_COCOA_H_

#import <Cocoa/Cocoa.h>

#include "chrome/browser/media_galleries/media_galleries_scan_result_dialog_controller.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_mac.h"

@class ConstrainedWindowAlert;
@class MediaGalleriesScanResultCocoaController;


namespace ui {
class MenuModel;
}

// This class displays an alert where the user selects which scan results
// (media folders) the app (extension) should have access to.
class MediaGalleriesScanResultDialogCocoa
    : public ConstrainedWindowMacDelegate,
      public MediaGalleriesScanResultDialog {
 public:
  MediaGalleriesScanResultDialogCocoa(
      MediaGalleriesScanResultDialogController* controller,
      MediaGalleriesScanResultCocoaController* delegate);
  virtual ~MediaGalleriesScanResultDialogCocoa();

  // Called when the user clicks the accept button.
  void OnAcceptClicked();
  // Called when the user clicks the cancel button.
  void OnCancelClicked();
  // Called when the user toggles a gallery checkbox.
  void OnCheckboxToggled(NSButton* checkbox);
  // Called when the user toggles a gallery checkbox.
  void OnFolderViewClicked(NSButton* folder_viewer_button);

  // MediaGalleriesScanResultDialog implementation:
  virtual void UpdateResults() OVERRIDE;

  // ConstrainedWindowMacDelegate implementation.
  virtual void OnConstrainedWindowClosed(
      ConstrainedWindowMac* window) OVERRIDE;

  ui::MenuModel* GetContextMenu(MediaGalleryPrefId prefid);

 private:
  friend class MediaGalleriesScanResultDialogCocoaTest;

  void UpdateScanResultCheckbox(const MediaGalleryPrefInfo& scan_result,
                                bool checked,
                                CGFloat y_pos);

  void InitDialogControls();
  CGFloat CreateCheckboxes(
      CGFloat y_pos,
      const MediaGalleriesScanResultDialogController::OrderedScanResults&
          scan_results);

  // MediaGalleriesScanResultDialog implementation:
  virtual void AcceptDialogForTesting() OVERRIDE;

  MediaGalleriesScanResultDialogController* controller_;  // weak
  scoped_ptr<ConstrainedWindowMac> window_;

  // The alert that the dialog is being displayed as.
  base::scoped_nsobject<ConstrainedWindowAlert> alert_;

  // True if the user has pressed accept.
  bool accepted_;

  // List of checkboxes ordered from bottom to top.
  base::scoped_nsobject<NSMutableArray> checkboxes_;

  // Container view for checkboxes.
  base::scoped_nsobject<NSView> checkbox_container_;

  // Container view for the main dialog contents.
  base::scoped_nsobject<NSBox> main_container_;

  // An Objective-C class to route callbacks from Cocoa code.
  base::scoped_nsobject<MediaGalleriesScanResultCocoaController>
      cocoa_controller_;

  DISALLOW_COPY_AND_ASSIGN(MediaGalleriesScanResultDialogCocoa);
};

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_MEDIA_GALLERIES_SCAN_RESULT_DIALOG_COCOA_H_
