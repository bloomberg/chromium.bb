// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

// StyledTextFieldCell customizes the look of the standard Cocoa text field.
// The border and focus ring are modified, as is the font baseline.  Subclasses
// can override |drawInteriorWithFrame:inView:| to provide custom drawing for
// decorations, but they must make sure to call the superclass' implementation
// with a modified frame after performing any custom drawing.

@interface StyledTextFieldCell : NSTextFieldCell {
}

// Baseline adjust for the text in this cell.  Defaults to 0.  Subclasses should
// override as needed.
- (CGFloat)baselineAdjust;

// Return the portion of the cell to show the text cursor over.  The default
// implementation returns the full |cellFrame|.  Subclasses should override this
// method if they add any decorations.
- (NSRect)textCursorFrameForFrame:(NSRect)cellFrame;

// Return the portion of the cell to use for text display.  This corresponds to
// the frame with our added decorations sliced off.  The default implementation
// returns the full |cellFrame|, as by default there are no decorations.
// Subclasses should override this method if they add any decorations.
- (NSRect)textFrameForFrame:(NSRect)cellFrame;

@end
