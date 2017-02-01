// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTION_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTION_H_

#import <UIKit/UIKit.h>

// Data for a suggestions item, compatible with Objective-C.
@interface ContentSuggestion : NSObject

@property(nonatomic, copy) NSString* title;
@property(nonatomic, strong) UIImage* image;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTION_H_
