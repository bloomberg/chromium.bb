// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "chrome/browser/cocoa/infobar_text_field.h"

@implementation InfoBarTextField

- (BOOL)textView:(NSTextView*)aTextView
   clickedOnLink:(id)link
         atIndex:(NSUInteger)charIndex {
  if ([[self delegate] respondsToSelector:@selector(linkClicked)])
    [[self delegate] performSelector:@selector(linkClicked)];

  return YES;  // We handled the click, so Cocoa does not need to do anything.
}

@end
