// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/drag_and_drop_recorder.h"

#include "base/metrics/histogram_macros.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

#if defined(__IPHONE_11_0) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_11_0)

// Reported metrics for the content of drag events.
enum DragContentForReporting {
  UNKNOWN = 0,
  IMAGE = 1,
  URL = 2,
  TEXT = 3,
  COUNT
};

// Records metrics regarding the content of drag and drops.
void RecordDragAndDropContentHistogram(DragContentForReporting sample) {
  // Wrapping the macro in a function because it expands to a lot of code and
  // is called from multiple places.
  UMA_HISTOGRAM_ENUMERATION("IOS.DragAndDrop.DragContent", sample,
                            DragContentForReporting::COUNT);
}

// Records the DragAndDrop.DragContent histogram for a given |dropSession|.
void RecordDragTypesForSession(id<UIDropSession> dropSession)
    API_AVAILABLE(ios(11.0)) {
  static NSDictionary* classToSampleNameMap = @{
        [UIImage class] : @(DragContentForReporting::IMAGE),
        [NSURL class] : @(DragContentForReporting::URL),
        [NSString class] : @(DragContentForReporting::TEXT)
  };
  bool containsAKnownClass = false;
  // Report a histogram for every item contained in |dropSession|.
  for (Class klass in classToSampleNameMap) {
    if ([dropSession canLoadObjectsOfClass:klass]) {
      RecordDragAndDropContentHistogram(static_cast<DragContentForReporting>(
          [classToSampleNameMap[klass] intValue]));
      containsAKnownClass = true;
    }
  }
  if (!containsAKnownClass) {
    RecordDragAndDropContentHistogram(DragContentForReporting::UNKNOWN);
  }
}

#endif

}  // namespace

#if defined(__IPHONE_11_0) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_11_0)
@interface DragAndDropRecorder ()<UIDropInteractionDelegate> {
#else
@interface DragAndDropRecorder () {
#endif
  // The currently active drop sessions.
  NSHashTable* dropSessions_;
}
@end

@implementation DragAndDropRecorder

- (instancetype)initWithView:(UIView*)view {
  self = [super init];
  if (self) {
#if defined(__IPHONE_11_0) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_11_0)
    if (@available(iOS 11, *)) {
      dropSessions_ = [NSHashTable weakObjectsHashTable];
      UIDropInteraction* dropInteraction =
          [[UIDropInteraction alloc] initWithDelegate:self];
      [view addInteraction:dropInteraction];
    }
#endif
  }
  return self;
}

#if defined(__IPHONE_11_0) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_11_0)
- (BOOL)dropInteraction:(UIDropInteraction*)interaction
       canHandleSession:(id<UIDropSession>)session API_AVAILABLE(ios(11.0)) {
  // |-dropInteraction:canHandleSession:| can be called multiple times for the
  // same drop session.
  // Maintain a set of weak references to these sessions to make sure metrics
  // are recorded only once per drop session.
  if (![dropSessions_ containsObject:session]) {
    [dropSessions_ addObject:session];
    RecordDragTypesForSession(session);
  }
  // Return "NO" as the goal of this UIDropInteractionDelegate is to report
  // metrics and not intercept events.
  return NO;
}
#endif

@end
