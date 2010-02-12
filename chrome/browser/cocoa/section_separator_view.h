// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_SECTION_SEPARATOR_VIEW_
#define CHROME_BROWSER_COCOA_SECTION_SEPARATOR_VIEW_

#import <Cocoa/Cocoa.h>

// A view class that renders a gradient "section" separator.  The visual
// style is modelled similarly to iPhone table view separators.  This view
// paints a simple top-to-bottom gradient in its bounds of fixed gray values.
// Optionally, it also paints a "topline" and "baseline".  Default is to
// draw both topline and baseline, but these can be overridden.
// The user of the class can override the color of the base line and top line
// using the |baselineSeparatorColor| and |toplineSeparatorColor| properties.
@interface SectionSeparatorView : NSView {
 @private
  BOOL showBaseLine_;
  NSColor* baselineSeparatorColor_;
  BOOL showTopLine_;
  NSColor* toplineSeparatorColor_;
}

@property (nonatomic, assign) BOOL showBaseLine;
@property (nonatomic, retain) NSColor* baselineSeparatorColor;
@property (nonatomic, assign) BOOL showTopLine;
@property (nonatomic, retain) NSColor* toplineSeparatorColor;

@end

#endif // CHROME_BROWSER_COCOA_SECTION_SEPARATOR_VIEW_
