// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_MEDIA_GALLERIES_DIALOG_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_MEDIA_GALLERIES_DIALOG_COCOA_H_

#import <Cocoa/Cocoa.h>

#include "base/gtest_prod_util.h"
#include "chrome/browser/media_gallery/media_galleries_dialog_controller.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_mac.h"

@class ConstrainedWindowAlert;
@class MediaGalleriesCocoaController;

namespace chrome {

class MediaGalleriesDialogBrowserTest;
class MediaGalleriesDialogTest;

// This class displays an alert that can be used to grant permission for
// extensions to access a gallery (media folders).
class MediaGalleriesDialogCocoa : public ConstrainedWindowMacDelegate,
                                  public MediaGalleriesDialog {
 public:
  MediaGalleriesDialogCocoa(
      MediaGalleriesDialogController* controller,
      MediaGalleriesCocoaController* delegate);
  virtual ~MediaGalleriesDialogCocoa();

  // Called when the user clicks the accept button.
  void OnAcceptClicked();
  // Called when the user clicks the cancel button.
  void OnCancelClicked();
  // Called when the user clicks the Add Gallery button.
  void OnAddFolderClicked();
  // Called when the user toggles a gallery checkbox.
  void OnCheckboxToggled(NSButton* checkbox);

  // MediaGalleriesDialog implementation:
  virtual void UpdateGallery(const MediaGalleryPrefInfo* gallery,
                             bool permitted) OVERRIDE;
  virtual void ForgetGallery(const MediaGalleryPrefInfo* gallery) OVERRIDE;

  // ConstrainedWindowMacDelegate implementation.
  virtual void OnConstrainedWindowClosed(
      ConstrainedWindowMac* window) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(MediaGalleriesDialogBrowserTest, Close);
  FRIEND_TEST_ALL_PREFIXES(MediaGalleriesDialogTest, InitializeCheckboxes);
  FRIEND_TEST_ALL_PREFIXES(MediaGalleriesDialogTest, ToggleCheckboxes);
  FRIEND_TEST_ALL_PREFIXES(MediaGalleriesDialogTest, UpdateAdds);
  FRIEND_TEST_ALL_PREFIXES(MediaGalleriesDialogTest, ForgetDeletes);

  NSButton* CheckboxForGallery(const MediaGalleryPrefInfo* gallery);

  void UpdateGalleryCheckbox(NSButton* checkbox,
                             const MediaGalleryPrefInfo* gallery,
                             bool permitted);
  void UpdateCheckboxContainerFrame();

  MediaGalleriesDialogController* controller_;  // weak
  scoped_ptr<ConstrainedWindowMac> window_;

  // True if the user has pressed accept.
  bool accepted_;

  // List of checkboxes ordered from bottom to top.
  scoped_nsobject<NSMutableArray> checkboxes_;

  // Container view for checkboxes.
  scoped_nsobject<NSView> checkbox_container_;

  // The alert that the dialog is being displayed as.
  scoped_nsobject<ConstrainedWindowAlert> alert_;

  // An Objective-C class to route callbacks from Cocoa code.
  scoped_nsobject<MediaGalleriesCocoaController> cocoa_controller_;

  DISALLOW_COPY_AND_ASSIGN(MediaGalleriesDialogCocoa);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_MEDIA_GALLERIES_DIALOG_COCOA_H_
