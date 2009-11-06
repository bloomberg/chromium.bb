// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/find_bar_text_field.h"

#include "base/logging.h"
#import "chrome/browser/cocoa/find_bar_text_field_cell.h"

@implementation FindBarTextField

+ (Class)cellClass {
  return [FindBarTextFieldCell class];
}

- (void)awakeFromNib {
  DCHECK([[self cell] isKindOfClass:[FindBarTextFieldCell class]]);
}

- (FindBarTextFieldCell*)findBarTextFieldCell {
  DCHECK([[self cell] isKindOfClass:[FindBarTextFieldCell class]]);
  return static_cast<FindBarTextFieldCell*>([self cell]);
}

@end
