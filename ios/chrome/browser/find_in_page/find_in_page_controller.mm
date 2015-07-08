// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/find_in_page/find_in_page_controller.h"

#import <UIKit/UIKit.h>
#import <cmath>

#include "base/ios/ios_util.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/find_in_page/find_in_page_model.h"
#import "ios/chrome/browser/find_in_page/js_findinpage_manager.h"
#import "ios/chrome/browser/web/dom_altering_lock.h"
#import "ios/web/public/web_state/crw_web_view_proxy.h"
#import "ios/web/public/web_state/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"

NSString* const kFindBarTextFieldWillBecomeFirstResponderNotification =
    @"kFindBarTextFieldWillBecomeFirstResponderNotification";
NSString* const kFindBarTextFieldDidResignFirstResponderNotification =
    @"kFindBarTextFieldDidResignFirstResponderNotification";

namespace {
// The delay (in secs) after which the find in page string will be pumped again.
const NSTimeInterval kRecurringPumpDelay = .01;
}

@interface FindInPageController () <DOMAltering, CRWWebStateObserver>
// The find in page controller delegate.
@property(nonatomic, readonly) id<FindInPageControllerDelegate> delegate;
// The web view's scroll view.
@property(nonatomic, readonly) CRWWebViewScrollViewProxy* webViewScrollView;

// Convenience method to obtain UIPasteboardNameFind from UIPasteBoard.
- (UIPasteboard*)findPasteboard;
// Find in Page text field listeners.
- (void)findBarTextFieldWillBecomeFirstResponder:(NSNotification*)note;
- (void)findBarTextFieldDidResignFirstResponder:(NSNotification*)note;
// Keyboard listeners.
- (void)keyboardDidShow:(NSNotification*)note;
- (void)keyboardWillHide:(NSNotification*)note;
// Constantly injects the find string in page until
// |disableFindInPageWithCompletionHandler:| is called or the find operation is
// complete. Calls |completionHandler| if the find operation is complete.
// |completionHandler| can be nil.
- (void)startPumpingWithCompletionHandler:(ProceduralBlock)completionHandler;
// Gives find in page more time to complete. Calls |completionHandler| with
// a BOOL indicating if the find operation was successfull. |completionHandler|
// can be nil.
- (void)pumpFindStringInPageWithCompletionHandler:
    (void (^)(BOOL))completionHandler;
// Processes the result of a single find in page pump. Calls |completionHandler|
// if pumping is done. Re-pumps if necessary.
- (void)processPumpResult:(BOOL)finished
              scrollPoint:(CGPoint)scrollPoint
        completionHandler:(ProceduralBlock)completionHandler;
// Prevent scrolling past the end of the page.
- (CGPoint)limitOverscroll:(CRWWebViewScrollViewProxy*)scrollViewProxy
                   atPoint:(CGPoint)point;
// Returns the associated web state. May be null.
- (web::WebState*)webState;
@end

@implementation FindInPageController {
 @private
  // Object that manages find_in_page.js injection into the web view.
  JsFindinpageManager* _findInPageJsManager;
  id<FindInPageControllerDelegate> _delegate;

  // Access to the web view from the web state.
  base::scoped_nsprotocol<id<CRWWebViewProxy>> _webViewProxy;

  // True when a find is in progress. Used to avoid running JavaScript during
  // disable when there is nothing to clear.
  BOOL _findStringStarted;

  // Bridge to observe the web state from Objective-C.
  scoped_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
}

@synthesize delegate = _delegate;

- (id)initWithWebState:(web::WebState*)webState
              delegate:(id<FindInPageControllerDelegate>)delegate {
  self = [super init];
  if (self) {
    DCHECK(delegate);
    _findInPageJsManager = base::mac::ObjCCastStrict<JsFindinpageManager>(
        [webState->GetJSInjectionReceiver()
            instanceOfClass:[JsFindinpageManager class]]);
    _delegate = delegate;
    _webStateObserverBridge.reset(
        new web::WebStateObserverBridge(webState, self));
    _webViewProxy.reset([webState->GetWebViewProxy() retain]);
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(findBarTextFieldWillBecomeFirstResponder:)
               name:kFindBarTextFieldWillBecomeFirstResponderNotification
             object:nil];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(findBarTextFieldDidResignFirstResponder:)
               name:kFindBarTextFieldDidResignFirstResponderNotification
             object:nil];
    DOMAlteringLock::CreateForWebState(webState);
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (FindInPageModel*)findInPageModel {
  return [_findInPageJsManager findInPageModel];
}

- (BOOL)canFindInPage {
  return [_webViewProxy hasSearchableTextContent];
}

- (void)initFindInPage {
  [_findInPageJsManager inject];

  // Initialize the module with our frame size.
  CGRect frame = [_webViewProxy bounds];
  [_findInPageJsManager setWidth:frame.size.width height:frame.size.height];
}

- (CRWWebViewScrollViewProxy*)webViewScrollView {
  return [_webViewProxy scrollViewProxy];
}

- (CGPoint)limitOverscroll:(CRWWebViewScrollViewProxy*)scrollViewProxy
                   atPoint:(CGPoint)point {
  CGFloat contentHeight = scrollViewProxy.contentSize.height;
  CGFloat frameHeight = scrollViewProxy.frame.size.height;
  CGFloat maxScroll = std::max<CGFloat>(0, contentHeight - frameHeight);
  if (point.y > maxScroll) {
    point.y = maxScroll;
  }
  return point;
}

- (void)processPumpResult:(BOOL)finished
              scrollPoint:(CGPoint)scrollPoint
        completionHandler:(ProceduralBlock)completionHandler {
  if (finished) {
    [_delegate willAdjustScrollPosition];
    scrollPoint = [self limitOverscroll:[_webViewProxy scrollViewProxy]
                                atPoint:scrollPoint];
    [[_webViewProxy scrollViewProxy] setContentOffset:scrollPoint animated:YES];
    if (completionHandler)
      completionHandler();
  } else {
    [self performSelector:@selector(startPumpingWithCompletionHandler:)
               withObject:completionHandler
               afterDelay:kRecurringPumpDelay];
  }
}

- (void)findStringInPage:(NSString*)query
       completionHandler:(ProceduralBlock)completionHandler {
  ProceduralBlockWithBool lockAction = ^(BOOL hasLock) {
    if (!hasLock) {
      if (completionHandler) {
        completionHandler();
      }
      return;
    }
    // Cancel any previous pumping.
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    [self initFindInPage];
    // Keep track of whether a find is in progress so to avoid running
    // JavaScript during disable if unnecessary.
    _findStringStarted = YES;
    base::WeakNSObject<FindInPageController> weakSelf(self);
    [_findInPageJsManager findString:query
                   completionHandler:^(BOOL finished, CGPoint point) {
                     [weakSelf processPumpResult:finished
                                     scrollPoint:point
                               completionHandler:completionHandler];
                   }];
  };
  DOMAlteringLock::FromWebState([self webState])->Acquire(self, lockAction);
}

- (void)startPumpingWithCompletionHandler:(ProceduralBlock)completionHandler {
  base::WeakNSObject<FindInPageController> weakSelf(self);
  id completionHandlerBlock = ^void(BOOL findFinished) {
    if (findFinished) {
      // Pumping complete. Nothing else to do.
      if (completionHandler)
        completionHandler();
      return;
    }
    // Further pumping is required.
    [weakSelf performSelector:@selector(startPumpingWithCompletionHandler:)
                   withObject:completionHandler
                   afterDelay:kRecurringPumpDelay];
  };
  [self pumpFindStringInPageWithCompletionHandler:completionHandlerBlock];
}

- (void)pumpFindStringInPageWithCompletionHandler:
    (void (^)(BOOL))completionHandler {
  base::WeakNSObject<FindInPageController> weakSelf(self);
  [_findInPageJsManager pumpWithCompletionHandler:^(BOOL finished,
                                                    CGPoint point) {
    base::scoped_nsobject<FindInPageController> strongSelf([weakSelf retain]);
    if (finished) {
      [[strongSelf delegate] willAdjustScrollPosition];
      point = [strongSelf limitOverscroll:[strongSelf webViewScrollView]
                                  atPoint:point];
      [[strongSelf webViewScrollView] setContentOffset:point animated:YES];
    }
    completionHandler(finished);
  }];
}

- (void)findNextStringInPageWithCompletionHandler:
    (ProceduralBlock)completionHandler {
  [self initFindInPage];
  base::WeakNSObject<FindInPageController> weakSelf(self);
  [_findInPageJsManager nextMatchWithCompletionHandler:^(CGPoint point) {
    base::scoped_nsobject<FindInPageController> strongSelf([weakSelf retain]);
    [[strongSelf delegate] willAdjustScrollPosition];
    point = [strongSelf limitOverscroll:[strongSelf webViewScrollView]
                                atPoint:point];
    [[strongSelf webViewScrollView] setContentOffset:point animated:YES];
    if (completionHandler)
      completionHandler();
  }];
}

// Highlight the previous search match, update model and scroll to match.
- (void)findPreviousStringInPageWithCompletionHandler:
    (ProceduralBlock)completionHandler {
  [self initFindInPage];
  base::WeakNSObject<FindInPageController> weakSelf(self);
  [_findInPageJsManager previousMatchWithCompletionHandler:^(CGPoint point) {
    base::scoped_nsobject<FindInPageController> strongSelf([weakSelf retain]);
    [[strongSelf delegate] willAdjustScrollPosition];
    point = [strongSelf limitOverscroll:[strongSelf webViewScrollView]
                                atPoint:point];
    [[strongSelf webViewScrollView] setContentOffset:point animated:YES];
    if (completionHandler)
      completionHandler();
  }];
}

// Remove highlights from the page and disable the model.
- (void)disableFindInPageWithCompletionHandler:
    (ProceduralBlock)completionHandler {
  if (![self canFindInPage])
    return;
  // Cancel any queued calls to |recurringPumpWithCompletionHandler|.
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
  base::WeakNSObject<FindInPageController> weakSelf(self);
  ProceduralBlock handler = ^{
    base::scoped_nsobject<FindInPageController> strongSelf([weakSelf retain]);
    if (strongSelf) {
      [strongSelf.get().findInPageModel setEnabled:NO];
      web::WebState* webState = [strongSelf webState];
      if (webState)
        DOMAlteringLock::FromWebState(webState)->Release(strongSelf);
    }
    if (completionHandler)
      completionHandler();
  };
  // Only run JSFindInPageManager disable if there is a string in progress to
  // avoid WKWebView crash on deallocation due to outstanding completion
  // handler.
  if (_findStringStarted) {
    [_findInPageJsManager disableWithCompletionHandler:handler];
    _findStringStarted = NO;
  } else {
    handler();
  }
}

- (void)saveSearchTerm {
  [self findPasteboard].string = [[self findInPageModel] text];
}

- (void)restoreSearchTerm {
  NSString* term = [self findPasteboard].string;
  [[self findInPageModel] updateQuery:(term ? term : @"") matches:0];
}

- (UIPasteboard*)findPasteboard {
  return [UIPasteboard pasteboardWithName:UIPasteboardNameFind create:NO];
}

- (web::WebState*)webState {
  return _webStateObserverBridge ? _webStateObserverBridge->web_state()
                                 : nullptr;
}

#pragma mark - Notification listeners

- (void)findBarTextFieldWillBecomeFirstResponder:(NSNotification*)note {
  // Listen to the keyboard appearance notifications.
  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter addObserver:self
                    selector:@selector(keyboardDidShow:)
                        name:UIKeyboardDidShowNotification
                      object:nil];
  [defaultCenter addObserver:self
                    selector:@selector(keyboardWillHide:)
                        name:UIKeyboardWillHideNotification
                      object:nil];
}

- (void)findBarTextFieldDidResignFirstResponder:(NSNotification*)note {
  // Resign from the keyboard appearance notifications on the next turn of the
  // runloop.
  dispatch_async(dispatch_get_main_queue(), ^{
    NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
    [defaultCenter removeObserver:self
                             name:UIKeyboardDidShowNotification
                           object:nil];
    [defaultCenter removeObserver:self
                             name:UIKeyboardWillHideNotification
                           object:nil];
  });
}

- (void)keyboardDidShow:(NSNotification*)note {
  NSDictionary* info = [note userInfo];
  CGSize kbSize =
      [[info objectForKey:UIKeyboardFrameEndUserInfoKey] CGRectValue].size;
  UIInterfaceOrientation orientation =
      [[UIApplication sharedApplication] statusBarOrientation];
  CGFloat kbHeight = kbSize.height;
  // Prior to iOS 8, the keyboard frame was not dependent on interface
  // orientation, so height and width need to be swapped in landscape mode.
  if (UIInterfaceOrientationIsLandscape(orientation) &&
      !base::ios::IsRunningOnIOS8OrLater()) {
    kbHeight = kbSize.width;
  }

  UIEdgeInsets insets = UIEdgeInsetsZero;
  insets.bottom = kbHeight;
  [_webViewProxy registerInsets:insets forCaller:self];
}

- (void)keyboardWillHide:(NSNotification*)note {
  [_webViewProxy unregisterInsetsForCaller:self];
}

- (void)detachFromWebState {
  _webStateObserverBridge.reset();
}

#pragma mark - CRWWebStateObserver Methods

- (void)webStateDestroyed:(web::WebState*)webState {
  [self detachFromWebState];
}

#pragma mark - DOMAltering Methods

- (BOOL)canReleaseDOMLock {
  return NO;
}

- (void)releaseDOMLockWithCompletionHandler:(ProceduralBlock)completionHandler {
  NOTREACHED();
}

@end
