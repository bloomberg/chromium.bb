// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/reading_list/reading_list_data_source.h"

class ReadingListModel;

// Mediator between the Model and the UI.
@interface ReadingListMediator : NSObject<ReadingListDataSource>

- (nullable instancetype)init NS_UNAVAILABLE;

- (nullable instancetype)initWithModel:(nonnull ReadingListModel*)model
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_MEDIATOR_H_
