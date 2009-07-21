// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/autocomplete_text_field.h"

#include "base/logging.h"
#include "chrome/browser/cocoa/autocomplete_text_field_cell.h"

@implementation AutocompleteTextField

+ (Class)cellClass {
  return [AutocompleteTextFieldCell class];
}

- (void)awakeFromNib {
  DCHECK([[self cell] isKindOfClass:[AutocompleteTextFieldCell class]]);
}

- (BOOL)textShouldPaste:(NSText*)fieldEditor {
  id delegate = [self delegate];
  if ([delegate respondsToSelector:@selector(control:textShouldPaste:)]) {
    return [delegate control:self textShouldPaste:fieldEditor];
  }
  return YES;
}

- (void)flagsChanged:(NSEvent*)theEvent {
  id delegate = [self delegate];
  if ([delegate respondsToSelector:@selector(control:flagsChanged:)]) {
    [delegate control:self flagsChanged:theEvent];
  }
  [super flagsChanged:theEvent];
}

@end
