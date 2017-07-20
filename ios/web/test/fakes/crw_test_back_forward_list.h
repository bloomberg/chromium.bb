// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_CRW_TEST_BACK_FORWARD_LIST_H_
#define IOS_WEB_NAVIGATION_CRW_TEST_BACK_FORWARD_LIST_H_

#import <Foundation/Foundation.h>

@class WKBackForwardListItem;

// A CRWTestBackForwardList can be used to stub out WKBackForwardList in tests.
@interface CRWTestBackForwardList : NSObject

// WKBackForwardList interface
@property(nullable, nonatomic, copy) NSArray<WKBackForwardListItem*>* backList;
@property(nullable, nonatomic, copy)
    NSArray<WKBackForwardListItem*>* forwardList;
@property(nullable, nonatomic, strong) WKBackForwardListItem* currentItem;
- (nullable WKBackForwardListItem*)itemAtIndex:(NSInteger)index;

// Resets this instance to simulate a session with no back/forward history, and
// a single entry for |currentItemURL| if it is not nil. If |currentItemURL| is
// nil, the session is reset to empty.
- (void)setCurrentURL:(nullable NSString*)currentItemURL;

// Resets this instance to simulate a session with the current entry at
// |currentItemURL|, and back and forward history entries as specified in
// |backListURLs| and |forwardListURLs|.
- (void)setCurrentURL:(nonnull NSString*)currentItemURL
         backListURLs:(nullable NSArray<NSString*>*)backListURLs
      forwardListURLs:(nullable NSArray<NSString*>*)forwardListURLs;
@end

#endif  // IOS_WEB_NAVIGATION_CRW_TEST_BACK_FORWARD_LIST_H_
