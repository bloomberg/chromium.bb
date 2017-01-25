// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/shell/shell_view_controller.h"

#import "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/web_view/public/criwv.h"
#import "ios/web_view/public/criwv_web_view.h"
#import "ios/web_view/shell/translate_controller.h"

namespace {
const CGFloat kButtonSize = 44;
}

@interface ShellViewController () {
  base::scoped_nsobject<UITextField> _field;
  base::scoped_nsprotocol<id<CRIWVWebView>> _webView;
  base::scoped_nsobject<TranslateController> _translateController;
}
- (void)back;
- (void)forward;
- (void)stopLoading;
@end

@implementation ShellViewController

@synthesize containerView = _containerView;
@synthesize toolbarView = _toolbarView;

- (instancetype)init {
  self = [super initWithNibName:@"MainView" bundle:nil];
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // Set up the toolbar buttons.
  // Back.
  UIButton* back = [UIButton buttonWithType:UIButtonTypeCustom];
  [back setImage:[UIImage imageNamed:@"toolbar_back"]
        forState:UIControlStateNormal];
  [back setFrame:CGRectMake(0, 0, kButtonSize, kButtonSize)];
  UIEdgeInsets insets = UIEdgeInsetsMake(5, 5, 4, 4);
  [back setImageEdgeInsets:insets];
  [back setAutoresizingMask:UIViewAutoresizingFlexibleRightMargin];
  [back addTarget:self
                action:@selector(back)
      forControlEvents:UIControlEventTouchUpInside];

  // Forward.
  UIButton* forward = [UIButton buttonWithType:UIButtonTypeCustom];
  [forward setImage:[UIImage imageNamed:@"toolbar_forward"]
           forState:UIControlStateNormal];
  [forward setFrame:CGRectMake(kButtonSize, 0, kButtonSize, kButtonSize)];
  [forward setImageEdgeInsets:insets];
  [forward setAutoresizingMask:UIViewAutoresizingFlexibleRightMargin];
  [forward addTarget:self
                action:@selector(forward)
      forControlEvents:UIControlEventTouchUpInside];

  // Stop.
  UIButton* stop = [UIButton buttonWithType:UIButtonTypeCustom];
  [stop setImage:[UIImage imageNamed:@"toolbar_stop"]
        forState:UIControlStateNormal];
  [stop setFrame:CGRectMake(2 * kButtonSize, 0, kButtonSize, kButtonSize)];
  [stop setImageEdgeInsets:insets];
  [stop setAutoresizingMask:UIViewAutoresizingFlexibleRightMargin];
  [stop addTarget:self
                action:@selector(stopLoading)
      forControlEvents:UIControlEventTouchUpInside];

  // Text field.
  const int kButtonCount = 3;
  _field.reset([[UITextField alloc]
      initWithFrame:CGRectMake(kButtonCount * kButtonSize, 6,
                               CGRectGetWidth([_toolbarView frame]) -
                                   kButtonCount * kButtonSize - 10,
                               31)]);
  [_field setDelegate:self];
  [_field setBackground:[[UIImage imageNamed:@"textfield_background"]
                            resizableImageWithCapInsets:UIEdgeInsetsMake(
                                                            12, 12, 12, 12)]];
  [_field setAutoresizingMask:UIViewAutoresizingFlexibleWidth];
  [_field setKeyboardType:UIKeyboardTypeWebSearch];
  [_field setAutocorrectionType:UITextAutocorrectionTypeNo];
  [_field setClearButtonMode:UITextFieldViewModeWhileEditing];

  [_toolbarView addSubview:back];
  [_toolbarView addSubview:forward];
  [_toolbarView addSubview:stop];
  [_toolbarView addSubview:_field];

  _webView.reset([[CRIWV webView] retain]);
  [_webView setDelegate:self];
  [_containerView addSubview:[_webView view]];
  [_webView loadURL:[NSURL URLWithString:@"https://www.google.com/"]];
}

- (UIBarPosition)positionForBar:(id<UIBarPositioning>)bar {
  if (bar == _toolbarView) {
    return UIBarPositionTopAttached;
  }
  return UIBarPositionAny;
}

- (void)back {
  if ([_webView canGoBack]) {
    [_webView goBack];
  }
}

- (void)forward {
  if ([_webView canGoForward]) {
    [_webView goForward];
  }
}

- (void)stopLoading {
  [_webView stopLoading];
}

- (BOOL)textFieldShouldReturn:(UITextField*)field {
  [_webView loadURL:[NSURL URLWithString:[field text]]];
  [field resignFirstResponder];
  [self updateToolbar];
  return YES;
}

- (void)updateToolbar {
  // Do not update the URL if the text field is currently being edited.
  if ([_field isFirstResponder]) {
    return;
  }

  [_field setText:[[_webView visibleURL] absoluteString]];
}

#pragma mark CRIWVWebViewDelegate methods

- (void)webView:(id<CRIWVWebView>)webView
    didFinishLoadingWithURL:(NSURL*)url
                loadSuccess:(BOOL)loadSuccess {
  // TODO(crbug.com/679895): Add some visual indication that the page load has
  // finished.
  [self updateToolbar];
}

- (void)webView:(id<CRIWVWebView>)webView
    didUpdateWithChanges:(CRIWVWebViewUpdateType)changes {
  if (changes & CRIWVWebViewUpdateTypeProgress) {
    // TODO(crbug.com/679895): Add a progress indicator.
  }

  if (changes & CRIWVWebViewUpdateTypeTitle) {
    // TODO(crbug.com/679895): Add a title display.
  }

  if (changes & CRIWVWebViewUpdateTypeURL) {
    [self updateToolbar];
  }
}

- (id<CRIWVTranslateDelegate>)translateDelegate {
  if (!_translateController)
    _translateController.reset([[TranslateController alloc] init]);
  return _translateController;
}

@end
