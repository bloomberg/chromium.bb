// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_interaction_manager.h"

#include "base/mac/scoped_block.h"
#include "base/mac/scoped_nsobject.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#include "ios/public/provider/chrome/browser/signin/signin_error_provider.h"

@interface FakeAddAccountViewController : UIViewController {
  FakeChromeIdentityInteractionManager* _manager;  // Weak.
  base::scoped_nsobject<UIButton> _cancelButton;
  base::scoped_nsobject<UIButton> _signInButton;
}
@end

@interface FakeChromeIdentityInteractionManager () {
  base::mac::ScopedBlock<SigninCompletionCallback> _completionCallback;
  base::scoped_nsobject<UIViewController> _viewController;
  BOOL _isCanceling;
}

- (void)addAccountViewControllerDidTapSignIn:(FakeAddAccountViewController*)vc;

- (void)addAccountViewControllerDidTapCancel:(FakeAddAccountViewController*)vc;

@end

@implementation FakeAddAccountViewController

- (instancetype)initWithInteractionManager:
    (FakeChromeIdentityInteractionManager*)manager {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _manager = manager;
  }
  return self;
}

- (void)dealloc {
  [_signInButton removeTarget:self
                       action:@selector(didTapSignIn:)
             forControlEvents:UIControlEventTouchUpInside];
  [_cancelButton removeTarget:self
                       action:@selector(didTapCancel:)
             forControlEvents:UIControlEventTouchUpInside];
  [super dealloc];
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // Obnoxious color, this is a test screen.
  self.view.backgroundColor = [UIColor magentaColor];

  _signInButton.reset([[UIButton buttonWithType:UIButtonTypeCustom] retain]);
  [_signInButton setTitle:@"Sign in" forState:UIControlStateNormal];
  [_signInButton addTarget:self
                    action:@selector(didTapSignIn:)
          forControlEvents:UIControlEventTouchUpInside];
  [self.view addSubview:_signInButton];

  _cancelButton.reset([[UIButton buttonWithType:UIButtonTypeCustom] retain]);
  [_cancelButton setTitle:@"Cancel" forState:UIControlStateNormal];
  [_cancelButton setAccessibilityIdentifier:@"cancel"];
  [_cancelButton addTarget:self
                    action:@selector(didTapCancel:)
          forControlEvents:UIControlEventTouchUpInside];
  [self.view addSubview:_cancelButton];
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];

  CGRect bounds = self.view.bounds;
  [_signInButton sizeToFit];
  [_signInButton
      setCenter:CGPointMake(CGRectGetMidX(bounds), CGRectGetMidY(bounds) - 50)];
  [_cancelButton sizeToFit];
  [_cancelButton
      setCenter:CGPointMake(CGRectGetMidX(bounds), CGRectGetMidY(bounds) + 50)];
}

- (void)didTapSignIn:(id)sender {
  [_manager addAccountViewControllerDidTapSignIn:self];
}

- (void)didTapCancel:(id)sender {
  [_manager addAccountViewControllerDidTapCancel:self];
}

@end

@implementation FakeChromeIdentityInteractionManager

@synthesize fakeIdentity = _fakeIdentity;

- (BOOL)isCanceling {
  return _isCanceling;
}

- (void)addAccountWithCompletion:(SigninCompletionCallback)completion {
  _completionCallback.reset(completion, base::scoped_policy::RETAIN);
  _viewController.reset(
      [[FakeAddAccountViewController alloc] initWithInteractionManager:self]);
  [self.delegate interactionManager:self
              presentViewController:_viewController
                           animated:YES
                         completion:nil];
}

- (void)reauthenticateUserWithID:(NSString*)userID
                           email:(NSString*)userEmail
                      completion:(SigninCompletionCallback)completion {
  [self addAccountWithCompletion:completion];
}

- (void)cancelAndDismissAnimated:(BOOL)animated {
  _isCanceling = YES;
  [self dismissAndRunCompletionCallbackWithError:[self canceledError]
                                        animated:animated];
  _isCanceling = NO;
}

- (void)addAccountViewControllerDidTapSignIn:(FakeAddAccountViewController*)vc {
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->AddIdentity(_fakeIdentity);
  [self dismissAndRunCompletionCallbackWithError:nil animated:YES];
}

- (void)addAccountViewControllerDidTapCancel:(FakeAddAccountViewController*)vc {
  [self dismissAndRunCompletionCallbackWithError:[self canceledError]
                                        animated:YES];
}

#pragma mark Helper

- (void)dismissAndRunCompletionCallbackWithError:(NSError*)error
                                        animated:(BOOL)animated {
  if (!_viewController) {
    [self runCompletionCallbackWithError:error];
    return;
  }
  [self.delegate interactionManager:self
      dismissViewControllerAnimated:animated
                         completion:^{
                           [self runCompletionCallbackWithError:error];
                         }];
}

- (void)runCompletionCallbackWithError:(NSError*)error {
  _viewController.reset();
  if (_completionCallback) {
    // Ensure self is not destroyed in the callback.
    base::scoped_nsobject<FakeChromeIdentityInteractionManager> strongSelf(
        [self retain]);
    _completionCallback.get()(error ? nil : _fakeIdentity, error);
    _completionCallback.reset();
  }
}

- (NSError*)canceledError {
  ios::SigninErrorProvider* provider =
      ios::GetChromeBrowserProvider()->GetSigninErrorProvider();
  return [NSError errorWithDomain:provider->GetSigninErrorDomain()
                             code:provider->GetCode(ios::SigninError::CANCELED)
                         userInfo:nil];
}

@end
