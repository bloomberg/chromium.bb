// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/autocomplete_text_field_editor.h"

#include "base/string_util.h"
#include "base/sys_string_conversions.h"

@implementation AutocompleteTextFieldEditor

- (void)copy:(id)sender {
  NSPasteboard* pb = [NSPasteboard generalPasteboard];
  [self performCopy:pb];
}

- (void)cut:(id)sender {
  NSPasteboard* pb = [NSPasteboard generalPasteboard];
  [self performCut:pb];
}

- (void)performCopy:(NSPasteboard*)pb {
  [pb declareTypes:[NSArray array] owner:nil];
  [self writeSelectionToPasteboard:pb types:
      [NSArray arrayWithObject:NSStringPboardType]];
}

- (void)performCut:(NSPasteboard*)pb {
  [self performCopy:pb];
  [self delete:nil];
}

- (BOOL)shouldPaste {
  id delegate = [self delegate];
  if (![delegate respondsToSelector:@selector(textShouldPaste:)] ||
      [delegate textShouldPaste:self]) {
    return YES;
  }
  return NO;
}

- (void)paste:(id)sender {
  if ([self shouldPaste]) {
    [super paste:sender];
  }
}

@end
