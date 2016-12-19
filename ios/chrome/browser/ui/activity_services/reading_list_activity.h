// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_READING_LIST_ACTIVITY_H_
#define IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_READING_LIST_ACTIVITY_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

class GURL;

// Activity that triggers the add-to-reading-list service.
@interface ReadingListActivity : UIActivity

// Identifier for the add-to-reading-list activity.
+ (NSString*)activityIdentifier;

- (instancetype)initWithURL:(const GURL&)activityURL
                      title:(NSString*)title
                  responder:(UIResponder*)responder;

@end

#endif  // IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_READING_LIST_ACTIVITY_H_
