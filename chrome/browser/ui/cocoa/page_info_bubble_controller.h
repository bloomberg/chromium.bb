// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/page_info_model.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"

// This NSWindowController subclass manages the InfoBubbleWindow and view that
// are displayed when the user clicks the security lock icon.
@interface PageInfoBubbleController : BaseBubbleController {
 @private
  // The model that generates the content displayed by the controller.
  scoped_ptr<PageInfoModel> model_;

  // Thin bridge that pushes model-changed notifications from C++ to Cocoa.
  scoped_ptr<PageInfoModel::PageInfoModelObserver> bridge_;

  // The certificate ID for the page, 0 if the page is not over HTTPS.
  int certID_;
}

@property(nonatomic, assign) int certID;

// Designated initializer. The new instance will take ownership of |model| and
// |bridge|. There should be a 1:1 mapping of models to bridges. The
// controller will release itself when the bubble is closed. |parentWindow|
// cannot be nil.
- (id)initWithPageInfoModel:(PageInfoModel*)model
              modelObserver:(PageInfoModel::PageInfoModelObserver*)bridge
               parentWindow:(NSWindow*)parentWindow;

// Shows the certificate display window. Note that this will implicitly close
// the bubble because the certificate window will become key. The certificate
// information attaches itself as a sheet to the |parentWindow|.
- (IBAction)showCertWindow:(id)sender;

// Opens the help center link that explains the contents of the page info.
- (IBAction)showHelpPage:(id)sender;

@end

@interface PageInfoBubbleController (ExposedForUnitTesting)
- (void)performLayout;
@end
