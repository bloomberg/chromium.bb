// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_DISCLOSURE_VIEW_CONTROLLER_
#define CHROME_BROWSER_COCOA_DISCLOSURE_VIEW_CONTROLLER_

#import <Cocoa/Cocoa.h>

@class DisclosureViewState;

// A view class that provides a disclosure triangle that controls the size
// of the view.  Toggling the disclosure triangle animates the change in
// size of the view.  The |openHeight| is initialized from the initial size
// of the view. |disclosureState| is initialized as |NSOnState| (of type
// NSCellStateValue) which corresponds to "open".
@interface DisclosureViewController : NSViewController {
 @private
  // The |disclosureState_| is instantiated from within |awakeFromNib|.
  // We do not hold it as a scoped_nsobject because it is exposed as a KVO
  // compliant property.
  DisclosureViewState* disclosureState_;  // strong reference
  CGFloat openHeight_;
}

@property (nonatomic, retain) DisclosureViewState* disclosureState;

@end

#endif // CHROME_BROWSER_COCOA_DISCLOSURE_VIEW_CONTROLLER_
