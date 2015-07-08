// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/find_in_page/js_findinpage_manager.h"

#include <string>

#import "base/ios/weak_nsobject.h"
#include "base/json/json_reader.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#import "ios/chrome/browser/find_in_page/find_in_page_model.h"

namespace {

// Global variable defined in find_in_page.js that can be used for testing
// whether JavaScript bas heen loaded.
NSString* const kFindInPageBeacon = @"window.__gCrWeb.findInPage";

// Initializes Find In Page JavaScript with the width and height of the window.
NSString* const kFindInPageInit = @"window.__gCrWeb.findInPage && "
                                   "window.__gCrWeb.findInPage.init(%.f, %.f);";

// This will only do verbatim matches.
// The timeout of 100ms is hardcoded into this string so we don't have
// to spend any time at runtime to format this constant into another constant.
NSString* const kFindInPageVerbatim =
    @"window.__gCrWeb.findInPage && "
     "window.__gCrWeb.findInPage.highlightWord(%@, false, 100.0);";

// The timeout of 100ms is hardcoded into this string so we don't have
// to spend any time at runtime to format this constant into another constant.
NSString* const kFindInPagePump =
    @"window.__gCrWeb.findInPage && "
     "window.__gCrWeb.findInPage.pumpSearch(100.0);";

NSString* const kFindInPagePrev = @"window.__gCrWeb.findInPage && "
                                   "window.__gCrWeb.findInPage.goPrev();";

NSString* const kFindInPageNext = @"window.__gCrWeb.findInPage && "
                                   "window.__gCrWeb.findInPage.goNext();";

NSString* const kFindInPageDisable = @"window.__gCrWeb.findInPage && "
                                      "window.__gCrWeb.findInPage.disable();";

NSString* const kFindInPagePending = @"[false]";

const FindInPageEntry kFindInPageEntryZero = {{0.0, 0.0}, 0};

}  // namespace

@interface JsFindinpageManager ()
// Update find in page model with results, return true if fip completes or
// false if still pending and requires pumping. If |point| is not nil, it will
// contain the scroll position upon return.
- (BOOL)processFindInPageResult:(NSString*)result
                 scrollPosition:(CGPoint*)point;
// Updates find in page model with results. Calls |completionHandler| with the
// the result of the processing and the new scroll position if successfull. If
// |completionHandler| is called with NO, further pumping is required.
// |completionHandler| cannot be nil.
- (void)processFindInPagePumpResult:(NSString*)result
                  completionHandler:(void (^)(BOOL, CGPoint))completionHandler;
// Helper functions to extract FindInPageEntry from JSON.
- (FindInPageEntry)findInPageEntryForJson:(NSString*)jsonStr;
- (FindInPageEntry)entryForListValue:(base::ListValue*)position;
// Executes |script| which is a piece of JavaScript to move to the next or
// previous element in the page and executes |completionHandler| after moving
// with the new scroll position passed in.
- (void)moveHighlightByEvaluatingJavaScript:(NSString*)script
                          completionHandler:
                              (void (^)(CGPoint))completionHandler;
// Updates the current match index and its found position in the model.
- (void)updateIndex:(NSInteger)index atPoint:(CGPoint)point;
@end

@implementation JsFindinpageManager

- (FindInPageModel*)findInPageModel {
  if (!findInPageModel_)
    findInPageModel_.reset([[FindInPageModel alloc] init]);
  return findInPageModel_.get();
}

- (void)setWidth:(CGFloat)width height:(CGFloat)height {
  NSString* javaScript =
      [NSString stringWithFormat:kFindInPageInit, width, height];
  [self evaluate:javaScript stringResultHandler:nil];
}

- (void)findString:(NSString*)query
    completionHandler:(void (^)(BOOL, CGPoint))completionHandler {
  DCHECK(completionHandler);
  // Save the query in the model before searching.
  [findInPageModel_ updateQuery:query matches:0];

  // Escape |query| before passing to js.
  std::string escapedJson;
  base::EscapeJSONString(base::SysNSStringToUTF16(query), true, &escapedJson);
  NSString* jsonQuery =
      [NSString stringWithFormat:kFindInPageVerbatim,
                                 base::SysUTF8ToNSString(escapedJson.c_str())];
  base::WeakNSObject<JsFindinpageManager> weakSelf(self);
  [self evaluate:jsonQuery
      stringResultHandler:^(NSString* result, NSError* error) {
        [weakSelf processFindInPagePumpResult:result
                            completionHandler:completionHandler];
      }];
}

- (void)pumpWithCompletionHandler:(void (^)(BOOL, CGPoint))completionHandler {
  DCHECK(completionHandler);
  base::WeakNSObject<JsFindinpageManager> weakSelf(self);
  [self evaluate:kFindInPagePump
      stringResultHandler:^(NSString* result, NSError* error) {
        // TODO(shreyasv): What to do here if this returns an NSError in the
        // WKWebView version.
        [weakSelf processFindInPagePumpResult:result
                            completionHandler:completionHandler];
      }];
}

- (void)nextMatchWithCompletionHandler:(void (^)(CGPoint))completionHandler {
  [self moveHighlightByEvaluatingJavaScript:kFindInPageNext
                          completionHandler:completionHandler];
}

- (void)previousMatchWithCompletionHandler:
        (void (^)(CGPoint))completionHandler {
  [self moveHighlightByEvaluatingJavaScript:kFindInPagePrev
                          completionHandler:completionHandler];
}

- (void)moveHighlightByEvaluatingJavaScript:(NSString*)script
                          completionHandler:
                              (void (^)(CGPoint))completionHandler {
  base::WeakNSObject<JsFindinpageManager> weakSelf(self);
  [self evaluate:script
      stringResultHandler:^(NSString* result, NSError* error) {
        base::WeakNSObject<JsFindinpageManager> strongSelf([weakSelf retain]);
        if (!strongSelf)
          return;
        DCHECK(!error);
        FindInPageEntry entry = kFindInPageEntryZero;
        if (![result isEqualToString:kFindInPagePending])
          entry = [strongSelf findInPageEntryForJson:result];
        CGPoint newPoint = entry.point;
        [strongSelf updateIndex:entry.index atPoint:newPoint];
        if (completionHandler)
          completionHandler(newPoint);
      }];
}

- (void)disableWithCompletionHandler:(ProceduralBlock)completionHandler {
  DCHECK(completionHandler);
  [self evaluate:kFindInPageDisable
      stringResultHandler:^(NSString* result, NSError* error) {
        completionHandler();
      }];
}

#pragma mark -
#pragma mark FindInPageEntry

- (BOOL)processFindInPageResult:(NSString*)result
                 scrollPosition:(CGPoint*)point {
  if (!result)
    return NO;

  // Parse JSONs.
  std::string json([result UTF8String]);
  scoped_ptr<base::Value> root(base::JSONReader::Read(json, false));
  if (!root.get())
    return YES;
  if (!root->IsType(base::Value::TYPE_LIST))
    return YES;

  base::ListValue* resultList = static_cast<base::ListValue*>(root.get());
  DCHECK(resultList);
  if (resultList) {
    if (resultList->GetSize() == 2) {
      int numHighlighted = 0;
      if (resultList->GetInteger(0, &numHighlighted)) {
        if (numHighlighted > 0) {
          base::ListValue* position;
          if (resultList->GetList(1, &position)) {
            [findInPageModel_ updateQuery:nil matches:numHighlighted];
            // Scroll to first match.
            FindInPageEntry entry = [self entryForListValue:position];
            [findInPageModel_ updateIndex:entry.index atPoint:entry.point];
            if (point)
              *point = entry.point;
          }
        }
      }
    }
  }
  return YES;
}

- (void)processFindInPagePumpResult:(NSString*)result
                  completionHandler:(void (^)(BOOL, CGPoint))completionHandler {
  CGPoint point = CGPointZero;
  if ([result isEqualToString:kFindInPagePending]) {
    completionHandler(NO, point);
  }
  // TODO(shreyasv): Inline this call from the logic from the above function
  // and remove the above function.
  BOOL processFIPResult =
      [self processFindInPageResult:result scrollPosition:&point];
  completionHandler(processFIPResult, point);
}

- (void)updateIndex:(NSInteger)index atPoint:(CGPoint)point {
  [findInPageModel_ updateIndex:index atPoint:point];
}

- (FindInPageEntry)findInPageEntryForJson:(NSString*)jsonStr {
  std::string json([jsonStr UTF8String]);
  scoped_ptr<base::Value> root(base::JSONReader::Read(json, false));
  if (!root.get())
    return kFindInPageEntryZero;

  if (!root->IsType(base::Value::TYPE_LIST))
    return kFindInPageEntryZero;

  base::ListValue* position = static_cast<base::ListValue*>(root.get());
  return [self entryForListValue:position];
}

- (FindInPageEntry)entryForListValue:(base::ListValue*)position {
  if (!position)
    return kFindInPageEntryZero;

  // Position should always be of length 3, from [index,x,y].
  DCHECK(position->GetSize() == 3);
  if (position->GetSize() != 3)
    return kFindInPageEntryZero;

  // The array position comes from the JSON string [index, x, y], which
  // represents the index of the currently found string, and the x and y
  // position necessary to center that string.  Pull out that data into a
  // FindInPageEntry struct.
  int index;
  double x = 0, y = 0;
  position->GetInteger(0, &index);
  position->GetDouble(1, &x);
  position->GetDouble(2, &y);
  FindInPageEntry entry;
  entry.index = index;
  entry.point.x = x;
  entry.point.y = y;
  return entry;
}

#pragma mark -
#pragma mark ProtectedMethods

- (NSString*)scriptPath {
  return @"find_in_page";
}

- (NSString*)presenceBeacon {
  return kFindInPageBeacon;
}

@end
