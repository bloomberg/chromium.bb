// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_web_view_resizer.h"

#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/web/public/features.h"
#import "ios/web/public/web_state/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FullscreenWebViewResizer ()
// The fullscreen model, used to get the information about the state of
// fullscreen.
@property(nonatomic, assign) FullscreenModel* model;
@end

@implementation FullscreenWebViewResizer

@synthesize model = _model;
@synthesize webState = _webState;

- (instancetype)initWithModel:(FullscreenModel*)model {
  // This can only be instantiated when the feature is enabled.
  if (!base::FeatureList::IsEnabled(web::features::kOutOfWebFullscreen))
    return nil;

  self = [super init];
  if (self) {
    _model = model;
  }
  return self;
}

- (void)dealloc {
  if (_webState && _webState->GetView())
    [_webState->GetView() removeObserver:self forKeyPath:@"frame"];
}

#pragma mark - Properties

- (void)setWebState:(web::WebState*)webState {
  if (_webState == webState)
    return;

  if (_webState && _webState->GetView())
    [_webState->GetView() removeObserver:self forKeyPath:@"frame"];

  _webState = webState;

  if (webState)
    [self observeWebStateViewFrame:webState];
}

#pragma mark - Public

- (void)updateForFullscreenProgress:(CGFloat)progress {
  if (!self.webState || !self.webState->GetView().superview)
    return;

  UIView* webView = self.webState->GetView();

  UIEdgeInsets newInsets =
      UIEdgeInsetsMake(self.model->GetCollapsedToolbarHeight() +
                           progress * (self.model->GetExpandedToolbarHeight() -
                                       self.model->GetCollapsedToolbarHeight()),
                       0, progress * self.model->GetBottomToolbarHeight(), 0);

  CGRect newFrame = UIEdgeInsetsInsetRect(webView.superview.bounds, newInsets);

  // Make sure the frame has changed to avoid a loop as the frame property is
  // actually monitored by this object.
  if (std::fabs(newFrame.origin.x - webView.frame.origin.x) < 0.01 &&
      std::fabs(newFrame.origin.y - webView.frame.origin.y) < 0.01 &&
      std::fabs(newFrame.size.width - webView.frame.size.width) < 0.01 &&
      std::fabs(newFrame.size.height - webView.frame.size.height) < 0.01)
    return;

  webView.frame = newFrame;
}

#pragma mark - Private

// Observes the frame property of the view of the |webState| using KVO.
- (void)observeWebStateViewFrame:(web::WebState*)webState {
  if (!webState->GetView())
    return;

  [webState->GetView() addObserver:self
                        forKeyPath:@"frame"
                           options:NSKeyValueObservingOptionInitial
                           context:nil];
}

// Callback for the KVO.
- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  if (![keyPath isEqualToString:@"frame"] || object != _webState->GetView())
    return;

  [self updateForFullscreenProgress:self.model->progress()];
}

@end
