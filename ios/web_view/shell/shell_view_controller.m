// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/shell/shell_view_controller.h"

#import <MobileCoreServices/MobileCoreServices.h>

#import "ios/web_view/public/cwv.h"
#import "ios/web_view/public/cwv_html_element.h"
#import "ios/web_view/public/cwv_web_view.h"
#import "ios/web_view/shell/translate_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Externed accessibility identifier.
NSString* const kWebViewShellJavaScriptDialogTextFieldAccessibiltyIdentifier =
    @"WebViewShellJavaScriptDialogTextFieldAccessibiltyIdentifier";

@interface ShellViewController ()<CWVUIDelegate,
                                  CWVWebViewDelegate,
                                  UITextFieldDelegate>
// Container for |webView|.
@property(nonatomic, strong) UIView* containerView;
// Text field used for navigating to URLs.
@property(nonatomic, strong) UITextField* field;
// Toolbar containing navigation buttons and |field|.
@property(nonatomic, strong) UIToolbar* toolbar;
// CWV view which renders the web page.
@property(nonatomic, strong) CWVWebView* webView;
// Handles the translation of the content displayed in |webView|.
@property(nonatomic, strong) TranslateController* translateController;

- (void)back;
- (void)forward;
- (void)stopLoading;
@end

@implementation ShellViewController

@synthesize containerView = _containerView;
@synthesize field = _field;
@synthesize toolbar = _toolbar;
@synthesize webView = _webView;
@synthesize translateController = _translateController;

- (void)viewDidLoad {
  [super viewDidLoad];

  CGRect bounds = self.view.bounds;

  // Set up the toolbar.
  self.toolbar = [[UIToolbar alloc] init];
  [_toolbar setBarTintColor:[UIColor colorWithRed:0.337
                                            green:0.467
                                             blue:0.988
                                            alpha:1.0]];
  [_toolbar setFrame:CGRectMake(0, 20, CGRectGetWidth(bounds), 44)];
  [_toolbar setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                                UIViewAutoresizingFlexibleBottomMargin];
  [self.view addSubview:_toolbar];

  // Set up the container view.
  self.containerView = [[UIView alloc] init];
  [_containerView setFrame:CGRectMake(0, 64, CGRectGetWidth(bounds),
                                      CGRectGetHeight(bounds) - 64)];
  [_containerView setBackgroundColor:[UIColor lightGrayColor]];
  [_containerView setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                                      UIViewAutoresizingFlexibleHeight];
  [self.view addSubview:_containerView];

  const int kButtonCount = 3;
  const CGFloat kButtonSize = 44;

  // Text field.
  self.field = [[UITextField alloc]
      initWithFrame:CGRectMake(kButtonCount * kButtonSize, 6,
                               CGRectGetWidth([_toolbar frame]) -
                                   kButtonCount * kButtonSize - 10,
                               31)];
  [_field setDelegate:self];
  [_field setBackground:[[UIImage imageNamed:@"textfield_background"]
                            resizableImageWithCapInsets:UIEdgeInsetsMake(
                                                            12, 12, 12, 12)]];
  [_field setAutoresizingMask:UIViewAutoresizingFlexibleWidth];
  [_field setKeyboardType:UIKeyboardTypeWebSearch];
  [_field setAutocorrectionType:UITextAutocorrectionTypeNo];
  [_field setClearButtonMode:UITextFieldViewModeWhileEditing];

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

  [_toolbar addSubview:back];
  [_toolbar addSubview:forward];
  [_toolbar addSubview:stop];
  [_toolbar addSubview:_field];

  self.webView = [CWV webViewWithFrame:[_containerView bounds]];
  [_webView setDelegate:self];
  [_webView setUIDelegate:self];
  [_webView setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                                UIViewAutoresizingFlexibleHeight];
  [_containerView addSubview:_webView];

  NSURLRequest* request = [NSURLRequest
      requestWithURL:[NSURL URLWithString:@"https://www.google.com/"]];
  [_webView loadRequest:request];
}

- (UIBarPosition)positionForBar:(id<UIBarPositioning>)bar {
  if (bar == _toolbar) {
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
  NSURLRequest* request =
      [NSURLRequest requestWithURL:[NSURL URLWithString:[field text]]];
  [_webView loadRequest:request];
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

#pragma mark CWVUIDelegate methods

- (void)webView:(CWVWebView*)webView
    runContextMenuWithTitle:(NSString*)menuTitle
             forHTMLElement:(CWVHTMLElement*)element
                     inView:(UIView*)view
        userGestureLocation:(CGPoint)location {
  if (!element.hyperlink) {
    return;
  }

  UIAlertController* alert = [UIAlertController
      alertControllerWithTitle:menuTitle
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];
  alert.popoverPresentationController.sourceView = view;
  alert.popoverPresentationController.sourceRect =
      CGRectMake(location.x, location.y, 1.0, 1.0);

  void (^copyHandler)(UIAlertAction*) = ^(UIAlertAction* action) {
    NSDictionary* item = @{
      (NSString*)(kUTTypeURL) : element.hyperlink,
      (NSString*)(kUTTypeUTF8PlainText) : [[element.hyperlink absoluteString]
          dataUsingEncoding:NSUTF8StringEncoding],
    };
    [[UIPasteboard generalPasteboard] setItems:@[ item ]];
  };
  [alert addAction:[UIAlertAction actionWithTitle:@"Copy Link"
                                            style:UIAlertActionStyleDefault
                                          handler:copyHandler]];

  [alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                            style:UIAlertActionStyleCancel
                                          handler:nil]];

  [self presentViewController:alert animated:YES completion:nil];
}

- (void)webView:(CWVWebView*)webView
    runJavaScriptAlertPanelWithMessage:(NSString*)message
                               pageURL:(NSURL*)URL
                     completionHandler:(void (^)(void))handler {
  UIAlertController* alert =
      [UIAlertController alertControllerWithTitle:nil
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];

  [alert addAction:[UIAlertAction actionWithTitle:@"Ok"
                                            style:UIAlertActionStyleDefault
                                          handler:^(UIAlertAction* action) {
                                            handler();
                                          }]];

  [self presentViewController:alert animated:YES completion:nil];
}

- (void)webView:(CWVWebView*)webView
    runJavaScriptConfirmPanelWithMessage:(NSString*)message
                                 pageURL:(NSURL*)URL
                       completionHandler:(void (^)(BOOL))handler {
  UIAlertController* alert =
      [UIAlertController alertControllerWithTitle:nil
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];

  [alert addAction:[UIAlertAction actionWithTitle:@"Ok"
                                            style:UIAlertActionStyleDefault
                                          handler:^(UIAlertAction* action) {
                                            handler(YES);
                                          }]];
  [alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                            style:UIAlertActionStyleCancel
                                          handler:^(UIAlertAction* action) {
                                            handler(NO);
                                          }]];

  [self presentViewController:alert animated:YES completion:nil];
}

- (void)webView:(CWVWebView*)webView
    runJavaScriptTextInputPanelWithPrompt:(NSString*)prompt
                              defaultText:(NSString*)defaultText
                                  pageURL:(NSURL*)URL
                        completionHandler:(void (^)(NSString*))handler {
  UIAlertController* alert =
      [UIAlertController alertControllerWithTitle:nil
                                          message:prompt
                                   preferredStyle:UIAlertControllerStyleAlert];

  [alert addTextFieldWithConfigurationHandler:^(UITextField* textField) {
    textField.text = defaultText;
    textField.accessibilityIdentifier =
        kWebViewShellJavaScriptDialogTextFieldAccessibiltyIdentifier;
  }];

  __weak UIAlertController* weakAlert = alert;
  [alert addAction:[UIAlertAction
                       actionWithTitle:@"Ok"
                                 style:UIAlertActionStyleDefault
                               handler:^(UIAlertAction* action) {
                                 NSString* textInput =
                                     weakAlert.textFields.firstObject.text;
                                 handler(textInput);
                               }]];
  [alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                            style:UIAlertActionStyleCancel
                                          handler:^(UIAlertAction* action) {
                                            handler(nil);
                                          }]];

  [self presentViewController:alert animated:YES completion:nil];
}

#pragma mark CWVWebViewDelegate methods

- (void)webView:(CWVWebView*)webView
    didFinishLoadingWithURL:(NSURL*)url
                loadSuccess:(BOOL)loadSuccess {
  // TODO(crbug.com/679895): Add some visual indication that the page load has
  // finished.
  [self updateToolbar];
}

- (void)webView:(CWVWebView*)webView
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

- (id<CWVTranslateDelegate>)translateDelegate {
  if (!_translateController)
    self.translateController = [[TranslateController alloc] init];
  return _translateController;
}

@end
