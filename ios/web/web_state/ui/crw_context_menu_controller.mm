// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_context_menu_controller.h"

#import <objc/runtime.h>
#include <stddef.h>

#include "base/ios/ios_util.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/web/public/web_state/context_menu_params.h"
#import "ios/web/public/web_state/js/crw_js_injection_evaluator.h"
#import "ios/web/public/web_state/ui/crw_context_menu_delegate.h"
#import "ios/web/web_state/context_menu_constants.h"
#import "ios/web/web_state/context_menu_params_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The long press detection duration must be shorter than the WKWebView's
// long click gesture recognizer's minimum duration. That is 0.55s.
// If our detection duration is shorter, our gesture recognizer will fire
// first, and if it fails the long click gesture (processed simultaneously)
// still is able to complete.
const NSTimeInterval kLongPressDurationSeconds = 0.55 - 0.1;

// If there is a movement bigger than |kLongPressMoveDeltaPixels|, the context
// menu will not be triggered.
const CGFloat kLongPressMoveDeltaPixels = 10.0;

// Cancels touch events for the given gesture recognizer.
void CancelTouches(UIGestureRecognizer* gesture_recognizer) {
  if (gesture_recognizer.enabled) {
    gesture_recognizer.enabled = NO;
    gesture_recognizer.enabled = YES;
  }
}
}  // namespace

@interface CRWContextMenuController ()<UIGestureRecognizerDelegate>

// The |webView|.
@property(nonatomic, readonly, weak) WKWebView* webView;
// The delegate that allow execute javascript.
@property(nonatomic, readonly, weak) id<CRWJSInjectionEvaluator>
    injectionEvaluator;
// The scroll view of |webView|.
@property(nonatomic, readonly, weak) id<CRWContextMenuDelegate> delegate;
// Returns the x, y offset the content has been scrolled.
@property(nonatomic, readonly) CGPoint scrollPosition;

// Returns a gesture recognizers with |fragment| in it's description.
- (UIGestureRecognizer*)gestureRecognizerWithDescriptionFragment:
    (NSString*)fragment;
// Called when the window has determined there was a long-press and context menu
// must be shown.
- (void)showContextMenu:(UIGestureRecognizer*)gestureRecognizer;
// Cancels all touch events in the web view (long presses, tapping, scrolling).
- (void)cancelAllTouches;
// Asynchronously fetches information about DOM element for the given point (in
// UIView coordinates). |handler| can not be nil. See |_DOMElementForLastTouch|
// for element format description.
- (void)fetchDOMElementAtPoint:(CGPoint)point
             completionHandler:(void (^)(NSDictionary*))handler;
// Sets the value of |_DOMElementForLastTouch|.
- (void)setDOMElementForLastTouch:(NSDictionary*)element;
// Forwards the execution of |script| to |javaScriptDelegate| and if it is nil,
// to |webView|.
- (void)executeJavaScript:(NSString*)script
        completionHandler:(web::JavaScriptResultBlock)completionHandler;
@end

@implementation CRWContextMenuController {
  // Long press recognizer that allows showing context menus.
  UILongPressGestureRecognizer* _contextMenuRecognizer;
  // DOM element information for the point where the user made the last touch.
  // Can be nil if has not been calculated yet. Precalculation is necessary
  // because retreiving DOM element relies on async API so element info can not
  // be built on demand. May contain the keys defined in
  // ios/web/web_state/context_menu_constants.h. All values are strings.
  NSDictionary* _DOMElementForLastTouch;
}

@synthesize delegate = _delegate;
@synthesize injectionEvaluator = _injectionEvaluator;
@synthesize webView = _webView;

- (instancetype)initWithWebView:(WKWebView*)webView
             injectionEvaluator:(id<CRWJSInjectionEvaluator>)injectionEvaluator
                       delegate:(id<CRWContextMenuDelegate>)delegate {
  DCHECK(webView);
  self = [super init];
  if (self) {
    _webView = webView;
    _delegate = delegate;
    _injectionEvaluator = injectionEvaluator;

    // The system context menu triggers after 0.55 second. Add a gesture
    // recognizer with a shorter delay to be able to cancel the system menu if
    // needed.
    _contextMenuRecognizer = [[UILongPressGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(showContextMenu:)];
    [_contextMenuRecognizer setMinimumPressDuration:kLongPressDurationSeconds];
    [_contextMenuRecognizer setAllowableMovement:kLongPressMoveDeltaPixels];
    [_contextMenuRecognizer setDelegate:self];
    [_webView addGestureRecognizer:_contextMenuRecognizer];

    if (base::ios::IsRunningOnIOS11OrLater()) {
      // WKWebView's default context menu gesture recognizer interferes with
      // the detection of a long press by |_contextMenuRecognizer|. WKWebView's
      // context menu gesture recognizer should fail if |_contextMenuRecognizer|
      // detects a long press.
      NSString* fragment = @"action=_longPressRecognized:";
      UIGestureRecognizer* systemContextMenuRecognizer =
          [self gestureRecognizerWithDescriptionFragment:fragment];
      if (systemContextMenuRecognizer) {
        [systemContextMenuRecognizer
            requireGestureRecognizerToFail:_contextMenuRecognizer];
        // requireGestureRecognizerToFail: doesn't retain the recognizer, so it
        // is possible for |iRecognizer| to outlive |recognizer| and end up with
        // a dangling pointer. Add a retaining associative reference to ensure
        // that the lifetimes work out.
        // Note that normally using the value as the key wouldn't make any
        // sense, but here it's fine since nothing needs to look up the value.
        void* associated_object_key = (__bridge void*)_contextMenuRecognizer;
        objc_setAssociatedObject(systemContextMenuRecognizer.view,
                                 associated_object_key, _contextMenuRecognizer,
                                 OBJC_ASSOCIATION_RETAIN_NONATOMIC);
      }
    }
  }
  return self;
}

- (UIGestureRecognizer*)gestureRecognizerWithDescriptionFragment:
    (NSString*)fragment {
  for (UIView* view in [[_webView scrollView] subviews]) {
    for (UIGestureRecognizer* recognizer in [view gestureRecognizers]) {
      if ([recognizer.description rangeOfString:fragment].length) {
        return recognizer;
      }
    }
  }
  return nil;
}

- (UIScrollView*)webScrollView {
  return [_webView scrollView];
}

- (CGPoint)scrollPosition {
  return self.webScrollView.contentOffset;
}

- (void)executeJavaScript:(NSString*)script
        completionHandler:(web::JavaScriptResultBlock)completionHandler {
  if (self.injectionEvaluator) {
    [self.injectionEvaluator executeJavaScript:script
                             completionHandler:completionHandler];
  } else {
    [self.webView evaluateJavaScript:script
                   completionHandler:completionHandler];
  }
}

- (void)showContextMenu:(UIGestureRecognizer*)gestureRecognizer {
  // If the gesture has already been handled, ignore it.
  if ([gestureRecognizer state] != UIGestureRecognizerStateBegan)
    return;

  if (![_DOMElementForLastTouch count])
    return;

  web::ContextMenuParams params =
      web::ContextMenuParamsFromElementDictionary(_DOMElementForLastTouch);
  params.view.reset(_webView);
  params.location = [gestureRecognizer locationInView:_webView];
  if ([_delegate webView:_webView handleContextMenu:params]) {
    // Cancelling all touches has the intended side effect of suppressing the
    // system's context menu.
    [self cancelAllTouches];
  }
}

- (void)cancelAllTouches {
  // Disable web view scrolling.
  CancelTouches(self.webView.scrollView.panGestureRecognizer);

  // All user gestures are handled by a subview of web view scroll view
  // (WKContentView).
  for (UIView* subview in self.webScrollView.subviews) {
    for (UIGestureRecognizer* recognizer in subview.gestureRecognizers) {
      CancelTouches(recognizer);
    }
  }

  // Just disabling/enabling the gesture recognizers is not enough to suppress
  // the click handlers on the JS side. This JS performs the function of
  // suppressing these handlers on the JS side.
  NSString* suppressNextClick = @"__gCrWeb.suppressNextClick()";
  [self executeJavaScript:suppressNextClick completionHandler:nil];
}

- (void)setDOMElementForLastTouch:(NSDictionary*)element {
  _DOMElementForLastTouch = [element copy];
}

#pragma mark -
#pragma mark UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  // Allows the custom UILongPressGestureRecognizer to fire simultaneously with
  // other recognizers.
  return YES;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
       shouldReceiveTouch:(UITouch*)touch {
  // Expect only _contextMenuRecognizer.
  DCHECK([gestureRecognizer isEqual:_contextMenuRecognizer]);

  // This is custom long press gesture recognizer. By the time the gesture is
  // recognized the web controller needs to know if there is a link under the
  // touch. If there a link, the web controller will reject system's context
  // menu and show another one. If for some reason context menu info is not
  // fetched - system context menu will be shown.
  [self setDOMElementForLastTouch:nil];
  __weak CRWContextMenuController* weakSelf = self;
  [self fetchDOMElementAtPoint:[touch locationInView:_webView]
             completionHandler:^(NSDictionary* element) {
               [weakSelf setDOMElementForLastTouch:element];
             }];
  return YES;
}

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer*)gestureRecognizer {
  // Expect only _contextMenuRecognizer.
  DCHECK([gestureRecognizer isEqual:_contextMenuRecognizer]);

  // Context menu should not be triggered while scrolling, as some users tend to
  // stop scrolling by resting the finger on the screen instead of touching the
  // screen. For more info, please refer to crbug.com/642375.
  if ([self webScrollView].dragging) {
    return NO;
  }

  // Fetching is considered as successful even if |_DOMElementForLastTouch| is
  // empty. However if |_DOMElementForLastTouch| is empty then custom context
  // menu should not be shown.
  UMA_HISTOGRAM_BOOLEAN("WebController.FetchContextMenuInfoAsyncSucceeded",
                        !!_DOMElementForLastTouch);
  return [_DOMElementForLastTouch count];
}

#pragma mark -
#pragma mark Web Page Features

- (void)fetchDOMElementAtPoint:(CGPoint)point
             completionHandler:(void (^)(NSDictionary*))handler {
  DCHECK(handler);
  CGPoint scrollOffset = self.scrollPosition;
  CGSize webViewContentSize = self.webScrollView.contentSize;
  CGFloat webViewContentWidth = webViewContentSize.width;
  CGFloat webViewContentHeight = webViewContentSize.height;
  NSString* getElementScript = [NSString
      stringWithFormat:@"__gCrWeb.getElementFromPoint(%g, %g, %g, %g);",
                       point.x + scrollOffset.x, point.y + scrollOffset.y,
                       webViewContentWidth, webViewContentHeight];
  [self executeJavaScript:getElementScript
        completionHandler:^(id element, NSError*) {
          handler(base::mac::ObjCCastStrict<NSDictionary>(element));
        }];
}

@end
