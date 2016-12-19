// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_side_swipe_provider.h"

#include "base/logging.h"
#include "components/reading_list/ios/reading_list_entry.h"
#include "components/reading_list/ios/reading_list_model.h"
#include "ios/chrome/browser/reading_list/reading_list_entry_loading_util.h"
#include "ios/web/public/web_state/web_state.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class ReadingListObserverBridge;

@interface ReadingListSideSwipeProvider () {
  // Keep a reference to detach before deallocing.
  ReadingListModel* _readingListModel;  // weak
}

@end

@implementation ReadingListSideSwipeProvider
- (instancetype)initWithReadingList:(ReadingListModel*)readingListModel {
  if (self = [super init]) {
    _readingListModel = readingListModel;
  }
  return self;
}

- (BOOL)canGoBack {
  return NO;
}

- (void)goBack:(web::WebState*)webState {
  NOTREACHED();
}

- (BOOL)canGoForward {
  return _readingListModel->unread_size() > 0;
}

- (UIImage*)paneIcon {
  return [UIImage imageNamed:@"reading_list_side_swipe"];
}

- (BOOL)rotateForwardIcon {
  return NO;
}

- (void)goForward:(web::WebState*)webState {
  if (!webState || _readingListModel->unread_size() == 0) {
    return;
  }
  int64_t updatetime = 0;
  const ReadingListEntry* first_entry = nullptr;
  for (const auto& url : _readingListModel->Keys()) {
    const ReadingListEntry* entry = _readingListModel->GetEntryByURL(url);
    if (!entry->IsRead() && entry->UpdateTime() > updatetime) {
      updatetime = entry->UpdateTime();
      first_entry = entry;
    }
  }
  DCHECK_GT(updatetime, 0);
  DCHECK_NE(first_entry, nullptr);
  reading_list::LoadReadingListEntry(*first_entry, _readingListModel, webState);
}

@end
