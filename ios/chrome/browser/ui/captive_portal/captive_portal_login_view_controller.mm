// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/captive_portal/captive_portal_login_view_controller.h"

#import <WebKit/WebKit.h>

#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/ui/material_components/utils.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#import "ios/third_party/material_components_ios/src/components/AppBar/src/MaterialAppBar.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const CGFloat kToolbarButtonWidth = 42.0f;

@interface CaptivePortalLoginViewController ()

@property(nonatomic, strong) NSURL* landingURL;
@property(nonatomic, strong) UILabel* landingPageURLLabel;
@property(nonatomic, strong) WKWebView* webView;

@property(nonatomic, strong) MDCAppBar* appBar;
@property(nonatomic, strong) UIBarButtonItem* backItem;
@property(nonatomic, strong) UIBarButtonItem* forwardItem;
@property(nonatomic, strong) UIBarButtonItem* doneItem;

// Creates and returns a button for navigating back in the web view history.
- (UIButton*)newBackButton;
// Creates and returns a button for navigating forward in the web view history.
- (UIButton*)newForwardButton;

// Called when the user taps on |doneItem|.
- (void)donePressed;

@end

@implementation CaptivePortalLoginViewController
@synthesize landingURL = _landingURL;
@synthesize landingPageURLLabel = _landingPageURLLabel;
@synthesize delegate = _delegate;
@synthesize webView = _webView;
@synthesize appBar = _appBar;
@synthesize backItem = _backItem;
@synthesize forwardItem = _forwardItem;
@synthesize doneItem = _doneItem;

- (instancetype)initWithLandingURL:(const GURL&)landingURL {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _landingURL = net::NSURLWithGURL(landingURL);
    _appBar = [[MDCAppBar alloc] init];
  }
  return self;
}

- (void)dealloc {
  [_webView removeObserver:self forKeyPath:@"canGoBack"];
  [_webView removeObserver:self forKeyPath:@"canGoForward"];
}

- (void)viewDidLoad {
  [super viewDidLoad];

  [self addChildViewController:[_appBar headerViewController]];

  WKWebViewConfiguration* configuration = [[WKWebViewConfiguration alloc] init];
  [configuration
      setWebsiteDataStore:[WKWebsiteDataStore nonPersistentDataStore]];
  self.webView = [[WKWebView alloc] initWithFrame:self.view.bounds
                                    configuration:configuration];
  [_webView setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                                UIViewAutoresizingFlexibleHeight];
  [self.view addSubview:_webView];

  ConfigureAppBarWithCardStyle(_appBar);
  [_appBar headerViewController].headerView.trackingScrollView =
      [_webView scrollView];
  [_webView scrollView].delegate = [_appBar headerViewController];

  // Add the app bar after the web view.
  [_appBar addSubviewsToParent];

  // Add the label after the AppBar.
  _landingPageURLLabel = [[UILabel alloc] init];
  _landingPageURLLabel.text =
      base::SysUTF16ToNSString(url_formatter::FormatUrlForSecurityDisplay(
          net::GURLWithNSURL(_landingURL)));
  _landingPageURLLabel.frame =
      CGRectMake(0.0, 20.0, CGRectGetWidth(self.view.frame), 20.0);
  [_landingPageURLLabel setAutoresizingMask:UIViewAutoresizingFlexibleWidth];
  _landingPageURLLabel.font = [MDCTypography captionFont];
  _landingPageURLLabel.textAlignment = NSTextAlignmentCenter;
  [self.view addSubview:_landingPageURLLabel];

  // Create a custom Done bar button item, as Material Navigation Bar does not
  // handle a system UIBarButtonSystemItemDone item.
  self.doneItem = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON)
              style:UIBarButtonItemStyleDone
             target:self
             action:@selector(donePressed)];
  // TODO(crbug.com/752216): The close button should dynamically change from
  // "cancel" (before the login is complete) to "done" (after the login is
  // complete).
  self.navigationItem.rightBarButtonItem = _doneItem;

  UIButton* backButton = [self newBackButton];
  self.backItem = [[UIBarButtonItem alloc] initWithCustomView:backButton];
  _backItem.width = kToolbarButtonWidth;
  _backItem.enabled = NO;

  UIButton* forwardButton = [self newForwardButton];
  self.forwardItem = [[UIBarButtonItem alloc] initWithCustomView:forwardButton];
  _forwardItem.width = kToolbarButtonWidth;
  _forwardItem.enabled = NO;

  self.navigationItem.leftBarButtonItems = @[ _backItem, _forwardItem ];

  // Observe web view canGoBack/canGoForward to update button state.
  [_webView addObserver:self
             forKeyPath:@"canGoBack"
                options:NSKeyValueObservingOptionNew
                context:nil];
  [_webView addObserver:self
             forKeyPath:@"canGoForward"
                options:NSKeyValueObservingOptionNew
                context:nil];

  [_webView loadRequest:[NSURLRequest requestWithURL:_landingURL]];
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSString*, id>*)change
                       context:(void*)context {
  _backItem.enabled = [_webView canGoBack];
  _forwardItem.enabled = [_webView canGoForward];
}

#pragma mark - Private methods

- (UIButton*)newBackButton {
  UIButton* backButton = [UIButton buttonWithType:UIButtonTypeCustom];
  [backButton addTarget:_webView
                 action:@selector(goBack)
       forControlEvents:UIControlEventTouchUpInside];
  [backButton setImage:NativeReversableImage(IDR_IOS_TOOLBAR_LIGHT_BACK, YES)
              forState:UIControlStateNormal];
  [backButton
      setImage:NativeReversableImage(IDR_IOS_TOOLBAR_LIGHT_BACK_PRESSED, YES)
      forState:UIControlStateHighlighted];
  [backButton
      setImage:NativeReversableImage(IDR_IOS_TOOLBAR_LIGHT_BACK_DISABLED, YES)
      forState:UIControlStateDisabled];
  backButton.accessibilityLabel = l10n_util::GetNSString(IDS_ACCNAME_BACK);
  return backButton;
}

- (UIButton*)newForwardButton {
  UIButton* forwardButton = [UIButton buttonWithType:UIButtonTypeCustom];
  [forwardButton addTarget:_webView
                    action:@selector(goForward)
          forControlEvents:UIControlEventTouchUpInside];
  [forwardButton
      setImage:NativeReversableImage(IDR_IOS_TOOLBAR_LIGHT_FORWARD, YES)
      forState:UIControlStateNormal];
  [forwardButton
      setImage:NativeReversableImage(IDR_IOS_TOOLBAR_LIGHT_FORWARD_PRESSED, YES)
      forState:UIControlStateHighlighted];
  [forwardButton setImage:NativeReversableImage(
                              IDR_IOS_TOOLBAR_LIGHT_FORWARD_DISABLED, YES)
                 forState:UIControlStateDisabled];
  forwardButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_ACCNAME_FORWARD);
  return forwardButton;
}

- (void)donePressed {
  [self.delegate captivePortalLoginViewControllerDidFinish:self];
}

@end
