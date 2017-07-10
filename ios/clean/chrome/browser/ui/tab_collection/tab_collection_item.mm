// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tab_collection/tab_collection_item.h"

#include "base/logging.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TabCollectionItem
@synthesize tabID = _tabID;
@synthesize title = _title;

#pragma mark - Public properties

- (void)setTabID:(NSString*)tabID {
  DCHECK_GE(tabID.length, 0U);
  _tabID = tabID;
}

- (void)setTitle:(NSString*)title {
  _title = title;
  if (title.length == 0) {
    _title = l10n_util::GetNSString(IDS_DEFAULT_TAB_TITLE);
  }
}

@end
