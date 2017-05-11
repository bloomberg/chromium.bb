// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/shell/shell_view_controller.h"

#import <ChromeWebView/ChromeWebView.h>
#import <MobileCoreServices/MobileCoreServices.h>

#import "ios/web_view/shell/shell_translation_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Externed accessibility identifier.
NSString* const kWebViewShellBackButtonAccessibilityLabel = @"Back";
NSString* const kWebViewShellForwardButtonAccessibilityLabel = @"Forward";
NSString* const kWebViewShellAddressFieldAccessibilityLabel = @"Address field";
NSString* const kWebViewShellJavaScriptDialogTextFieldAccessibiltyIdentifier =
    @"WebViewShellJavaScriptDialogTextFieldAccessibiltyIdentifier";

@interface ShellViewController ()<CWVNavigationDelegate,
                                  CWVUIDelegate,
                                  UITextFieldDelegate>
// Container for |webView|.
@property(nonatomic, strong) UIView* containerView;
// Text field used for navigating to URLs.
@property(nonatomic, strong) UITextField* field;
// Toolbar button to navigate backwards.
@property(nonatomic, strong) UIButton* backButton;
// Toolbar button to navigate forwards.
@property(nonatomic, strong) UIButton* forwardButton;
// Toolbar containing navigation buttons and |field|.
@property(nonatomic, strong) UIToolbar* toolbar;
// CWV view which renders the web page.
@property(nonatomic, strong) CWVWebView* webView;
// Handles the translation of the content displayed in |webView|.
@property(nonatomic, strong) ShellTranslationDelegate* translationDelegate;

- (void)back;
- (void)forward;
- (void)stopLoading;
// Disconnects and release the |webView|.
- (void)resetWebView;
@end

@implementation ShellViewController

@synthesize backButton = _backButton;
@synthesize containerView = _containerView;
@synthesize field = _field;
@synthesize forwardButton = _forwardButton;
@synthesize toolbar = _toolbar;
@synthesize webView = _webView;
@synthesize translationDelegate = _translationDelegate;

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

  const int kButtonCount = 4;
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
  [_field setAccessibilityLabel:kWebViewShellAddressFieldAccessibilityLabel];

  // Set up the toolbar buttons.
  // Back.
  self.backButton = [UIButton buttonWithType:UIButtonTypeCustom];
  [_backButton setImage:[UIImage imageNamed:@"toolbar_back"]
               forState:UIControlStateNormal];
  [_backButton setFrame:CGRectMake(0, 0, kButtonSize, kButtonSize)];
  UIEdgeInsets insets = UIEdgeInsetsMake(5, 5, 4, 4);
  [_backButton setImageEdgeInsets:insets];
  [_backButton setAutoresizingMask:UIViewAutoresizingFlexibleRightMargin];
  [_backButton addTarget:self
                  action:@selector(back)
        forControlEvents:UIControlEventTouchUpInside];
  [_backButton setAccessibilityLabel:kWebViewShellBackButtonAccessibilityLabel];

  // Forward.
  self.forwardButton = [UIButton buttonWithType:UIButtonTypeCustom];
  [_forwardButton setImage:[UIImage imageNamed:@"toolbar_forward"]
                  forState:UIControlStateNormal];
  [_forwardButton
      setFrame:CGRectMake(kButtonSize, 0, kButtonSize, kButtonSize)];
  [_forwardButton setImageEdgeInsets:insets];
  [_forwardButton setAutoresizingMask:UIViewAutoresizingFlexibleRightMargin];
  [_forwardButton addTarget:self
                     action:@selector(forward)
           forControlEvents:UIControlEventTouchUpInside];
  [_forwardButton
      setAccessibilityLabel:kWebViewShellForwardButtonAccessibilityLabel];

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

  // Menu.
  UIButton* menu = [UIButton buttonWithType:UIButtonTypeCustom];
  [menu setImage:[UIImage imageNamed:@"toolbar_more_horiz"]
        forState:UIControlStateNormal];
  [menu setFrame:CGRectMake(3 * kButtonSize, 0, kButtonSize, kButtonSize)];
  [menu setImageEdgeInsets:insets];
  [menu setAutoresizingMask:UIViewAutoresizingFlexibleRightMargin];
  [menu addTarget:self
                action:@selector(showMenu)
      forControlEvents:UIControlEventTouchUpInside];

  [_toolbar addSubview:_backButton];
  [_toolbar addSubview:_forwardButton];
  [_toolbar addSubview:stop];
  [_toolbar addSubview:menu];
  [_toolbar addSubview:_field];

  [CWVWebView setUserAgentProduct:@"Dummy/1.0"];

  CWVWebViewConfiguration* configuration =
      [CWVWebViewConfiguration defaultConfiguration];
  self.webView = [[CWVWebView alloc] initWithFrame:[_containerView bounds]
                                     configuration:configuration];
  // Gives a restoration identifier so that state restoration works.
  _webView.restorationIdentifier = @"webView";
  _webView.navigationDelegate = self;
  _webView.UIDelegate = self;
  _translationDelegate = [[ShellTranslationDelegate alloc] init];
  _webView.translationController.delegate = _translationDelegate;

  [_webView setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                                UIViewAutoresizingFlexibleHeight];
  [_containerView addSubview:_webView];

  [_webView addObserver:self
             forKeyPath:@"canGoBack"
                options:NSKeyValueObservingOptionNew
                context:nil];
  [_webView addObserver:self
             forKeyPath:@"canGoForward"
                options:NSKeyValueObservingOptionNew
                context:nil];

  NSURLRequest* request = [NSURLRequest
      requestWithURL:[NSURL URLWithString:@"https://www.google.com/"]];
  [_webView loadRequest:request];
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey, id>*)change
                       context:(void*)context {
  if ([keyPath isEqualToString:@"canGoBack"]) {
    _backButton.enabled = [_webView canGoBack];
  } else if ([keyPath isEqualToString:@"canGoForward"]) {
    _forwardButton.enabled = [_webView canGoForward];
  }
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

- (void)showMenu {
  UIAlertController* alertController = [UIAlertController
      alertControllerWithTitle:nil
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];
  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                         style:UIAlertActionStyleCancel
                                       handler:nil]];

  __weak ShellViewController* weakSelf = self;

  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Reload"
                                         style:UIAlertActionStyleDefault
                                       handler:^(UIAlertAction* action) {
                                         [weakSelf.webView reload];
                                       }]];

  // Removes the web view from the view hierarchy and releases it. For
  // testing deallocation behavior, because there have been multiple crash bugs
  // on deallocation of CWVWebView.
  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Deallocate web view"
                                         style:UIAlertActionStyleDefault
                                       handler:^(UIAlertAction* action) {
                                         [weakSelf resetWebView];
                                       }]];

  [self presentViewController:alertController animated:YES completion:nil];
}

- (void)resetWebView {
  [_webView removeFromSuperview];
  [_webView removeObserver:self forKeyPath:@"canGoBack"];
  [_webView removeObserver:self forKeyPath:@"canGoForward"];
  _webView = nil;
}

- (void)dealloc {
  [_webView removeObserver:self forKeyPath:@"canGoBack"];
  [_webView removeObserver:self forKeyPath:@"canGoForward"];
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

- (CWVWebView*)webView:(CWVWebView*)webView
    createWebViewWithConfiguration:(CWVWebViewConfiguration*)configuration
               forNavigationAction:(CWVNavigationAction*)action {
  NSLog(@"Create new CWVWebView for %@. User initiated? %@", action.request.URL,
        action.userInitiated ? @"Yes" : @"No");
  return nil;
}

- (void)webViewDidClose:(CWVWebView*)webView {
  NSLog(@"webViewDidClose");
}

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

#pragma mark CWVNavigationDelegate methods

- (BOOL)webView:(CWVWebView*)webView
    shouldStartLoadWithRequest:(NSURLRequest*)request {
  NSLog(@"shouldStartLoadWithRequest");
  return YES;
}

- (BOOL)webView:(CWVWebView*)webView
    shouldContinueLoadWithResponse:(NSURLResponse*)response {
  NSLog(@"shouldContinueLoadWithResponse");
  return YES;
}

- (void)webViewDidStartProvisionalNavigation:(CWVWebView*)webView {
  NSLog(@"webViewDidStartProvisionalNavigation");
  [self updateToolbar];
}

- (void)webViewDidCommitNavigation:(CWVWebView*)webView {
  NSLog(@"webViewDidCommitNavigation");
  [self updateToolbar];
}

- (void)webView:(CWVWebView*)webView didLoadPageWithSuccess:(BOOL)success {
  NSLog(@"webView:didLoadPageWithSuccess");
  // TODO(crbug.com/679895): Add some visual indication that the page load has
  // finished.
  [self updateToolbar];
}

- (void)webViewWebContentProcessDidTerminate:(CWVWebView*)webView {
  NSLog(@"webViewWebContentProcessDidTerminate");
}

@end
