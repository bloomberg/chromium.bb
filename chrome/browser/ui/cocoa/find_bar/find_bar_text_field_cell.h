// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/styled_text_field_cell.h"

#include "base/memory/scoped_nsobject.h"

// FindBarTextFieldCell extends StyledTextFieldCell to provide support for a
// results label rooted at the right edge of the cell.
@interface FindBarTextFieldCell : StyledTextFieldCell {
 @private
  // Set if there is a results label to display on the right side of the cell.
  scoped_nsobject<NSAttributedString> resultsString_;
}

// Sets the results label to the localized equivalent of "X of Y".
- (void)setActiveMatch:(NSInteger)current of:(NSInteger)total;

- (void)clearResults;

- (NSString*)resultsString;

@end
