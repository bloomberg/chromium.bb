// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTION_EXTRA_BUILDER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTION_EXTRA_BUILDER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/reading_list/reading_list_collection_view_item.h"

// Extra information available for the ContentSuggestion of type reading list.
@interface ContentSuggestionReadingListExtra : NSObject

+ (ContentSuggestionReadingListExtra*)extraWithDistillationStatus:
    (ReadingListUIDistillationStatus)status;

// Status of the distillation.
@property(nonatomic, assign) ReadingListUIDistillationStatus status;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTION_EXTRA_BUILDER_H_
