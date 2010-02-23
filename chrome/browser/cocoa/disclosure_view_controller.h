// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_DISCLOSURE_VIEW_CONTROLLER_
#define CHROME_BROWSER_COCOA_DISCLOSURE_VIEW_CONTROLLER_

#import <Cocoa/Cocoa.h>
#include "base/scoped_nsobject.h"

@class DisclosureViewState;
@class NSViewAnimation;

// A view class that provides a disclosure triangle that controls the size
// of the view.  Toggling the disclosure triangle animates the change in
// size of the view.  The |openHeight| is initialized from the initial size
// of the view. |disclosureState| is initialized as |NSOnState| (of type
// NSCellStateValue) which corresponds to "open".
@interface DisclosureViewController : NSViewController {
 @private
  // The |detailedView_| IBOutlet references the content that becomes visible
  // when the disclosure view is in the "open" state.  We hold a reference
  // to the content so that we can hide and show it when disclosure state
  // changes.
  IBOutlet NSBox* detailedView_;  // weak reference

  // The |disclosureState_| is instantiated from within |awakeFromNib|.
  // We do not hold it as a scoped_nsobject because it is exposed as a KVO
  // compliant property.
  DisclosureViewState* disclosureState_;  // strong reference

  // Open height determines the height of the disclosed view.  This value
  // is derived from the initial height specified in the nib.
  CGFloat openHeight_;

  // Value passed in to the designated initializer.  Used to set up
  // initial view state when we |awakeFromNib|.
  NSCellStateValue initialDisclosureState_;

  // Animation object for view disclosure transitions.
  scoped_nsobject<NSViewAnimation> animation_;
}

@property (nonatomic, retain) DisclosureViewState* disclosureState;

// Designated initializer.  Sets the initial disclosure state.
- (id)initWithNibName:(NSString *)nibNameOrNil
               bundle:(NSBundle *)nibBundleOrNil
           disclosure:(NSCellStateValue)disclosureState;

@end

#endif // CHROME_BROWSER_COCOA_DISCLOSURE_VIEW_CONTROLLER_
