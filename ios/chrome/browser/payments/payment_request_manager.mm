// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/payment_request_manager.h"

#include "base/ios/block_types.h"
#include "base/ios/ios_util.h"
#import "base/mac/bind_objc_block.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#import "base/values.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/ios/browser/autofill_driver_ios.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/payments/js_payment_request_manager.h"
#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/payments/payment_request_coordinator.h"
#include "ios/chrome/browser/procedural_block_types.h"
#include "ios/web/public/favicon_status.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/navigation_manager.h"
#include "ios/web/public/payments/payment_request.h"
#include "ios/web/public/ssl_status.h"
#import "ios/web/public/url_scheme_util.h"
#import "ios/web/public/web_state/crw_web_view_proxy.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#include "ios/web/public/web_state/url_verification_constants.h"
#include "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Command prefix for injected JavaScript.
const std::string kCommandPrefix = "paymentRequest";

// Time interval between attempts to unblock the webview's JS event queue.
const NSTimeInterval kNoopInterval = 0.1;

// Time interval before closing the UI if the page has not yet called
// PaymentResponse.complete().
const NSTimeInterval kTimeoutInterval = 60.0;

}  // namespace

@interface PaymentRequestManager ()<CRWWebStateObserver,
                                    PaymentRequestCoordinatorDelegate> {
  // View controller used to present the PaymentRequest view controller.
  __weak UIViewController* _baseViewController;

  // PersonalDataManager used to manage user credit cards and addresses.
  autofill::PersonalDataManager* _personalDataManager;

  // Object that owns an instance of web::PaymentRequest as provided by the page
  // invoking the PaymentRequest API. Also caches credit cards and addresses
  // provided by the _personalDataManager and manages selected ones for the
  // current PaymentRequest flow.
  std::unique_ptr<PaymentRequest> _paymentRequest;

  // WebState for the tab this object is attached to.
  web::WebState* _webState;

  // Observer for |_webState|.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;

  // Boolean to track if the current WebState is enabled (JS callback is set
  // up).
  BOOL _webStateEnabled;

  // Boolean to track if the script has been injected in the current page. This
  // is a faster check than asking the JS controller.
  BOOL _isScriptInjected;

  // True when close has been called and the PaymentRequest coordinator has
  // been destroyed.
  BOOL _closed;

  // Coordinator used to create and present the PaymentRequest view controller.
  PaymentRequestCoordinator* _paymentRequestCoordinator;

  // Timer used to periodically unblock the webview's JS event queue.
  NSTimer* _unblockEventQueueTimer;

  // Timer used to complete the Payment Request flow and close the UI if the
  // page does not call PaymentResponse.complete() in a timely fashion.
  NSTimer* _paymentResponseTimeoutTimer;

  // Timer used to cancel the Payment Request flow and close the UI if the
  // page does not settle the pending update promise in a timely fashion.
  NSTimer* _updateEventTimeoutTimer;
}

// Object that manages JavaScript injection into the web view.
@property(nonatomic, weak) JSPaymentRequestManager* paymentRequestJsManager;

// Synchronous method executed by -asynchronouslyEnablePaymentRequest:
- (void)doEnablePaymentRequest:(BOOL)enabled;

// Terminates the pending request with an error message and dismisses the UI.
// Invokes the callback once the request has been terminated.
- (void)terminateRequestWithErrorMessage:(NSString*)errorMessage
                                callback:(ProceduralBlockWithBool)callback;

// Initialize the PaymentRequest JavaScript.
- (void)initializeWebViewForPaymentRequest;

// Handler for injected JavaScript callbacks.
- (BOOL)handleScriptCommand:(const base::DictionaryValue&)JSONCommand;

// Handles invocations of PaymentRequest.show(). The value of the JavaScript
// PaymentRequest object should be provided in |message|. Returns YES if the
// invocation was successful.
- (BOOL)handleRequestShow:(const base::DictionaryValue&)message;

// Handles invocations of PaymentRequest.abort(). Returns YES if the invocation
// was successful.
- (BOOL)handleRequestAbort:(const base::DictionaryValue&)message;

// Called by |_updateEventTimeoutTimer|, displays an error message. Upon
// dismissal of the error message, cancels the Payment Request as if it was
// performend by the user.
- (BOOL)displayErrorThenCancelRequest;

// Called by |_paymentResponseTimeoutTimer|, invokes handleResponseComplete:
// as if PaymentResponse.complete() was invoked with the default "unknown"
// argument.
- (BOOL)doResponseComplete;

// Handles invocations of PaymentResponse.complete(). Returns YES if the
// invocation was successful.
- (BOOL)handleResponseComplete:(const base::DictionaryValue&)message;

// Handles invocations of PaymentRequestUpdateEvent.updateWith(). Returns YES if
// the invocation was successful.
- (BOOL)handleUpdatePaymentDetails:(const base::DictionaryValue&)message;

// Establishes a timer that periodically prompts the JS manager to execute a
// noop. This works around an issue where the JS event queue is blocked while
// presenting the Payment Request UI.
- (void)setUnblockEventQueueTimer;

// Establishes a timer that calls doResponseComplete when it times out. Per
// the spec, if the page does not call PaymentResponse.complete() within some
// timeout period, user agents may behave as if the complete() method was
// called with no arguments.
- (void)setPaymentResponseTimeoutTimer;

// Establishes a timer that dismisses the Payment Request UI when it times out.
// Per the spec, implementations may choose to consider a timeout for the
// promise provided with the PaymentRequestUpdateEvent.updateWith() call. If the
// promise doesn't get settled in a reasonable amount of time, it is as if it
// was rejected.
- (void)setUpdateEventTimeoutTimer;

@end

@implementation PaymentRequestManager

@synthesize enabled = _enabled;
@synthesize webState = _webState;
@synthesize browserState = _browserState;
@synthesize paymentRequestJsManager = _paymentRequestJsManager;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState {
  if ((self = [super init])) {
    _baseViewController = viewController;

    _browserState = browserState;

    _personalDataManager =
        autofill::PersonalDataManagerFactory::GetForBrowserState(
            browserState->GetOriginalChromeBrowserState());
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (void)setWebState:(web::WebState*)webState {
  [self disconnectWebState];
  if (webState) {
    _paymentRequestJsManager =
        base::mac::ObjCCastStrict<JSPaymentRequestManager>(
            [webState->GetJSInjectionReceiver()
                instanceOfClass:[JSPaymentRequestManager class]]);
    _webState = webState;
    _webStateObserver.reset(new web::WebStateObserverBridge(webState, self));
    [self enableCurrentWebState];
  } else {
    _webState = nullptr;
  }
}

- (void)enablePaymentRequest:(BOOL)enabled {
  // Asynchronously enables PaymentRequest, so that some preferences
  // (UIAccessibilityIsVoiceOverRunning(), for example) have time to synchronize
  // with their own notifications.
  __weak PaymentRequestManager* weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf doEnablePaymentRequest:enabled];
  });
}

- (void)doEnablePaymentRequest:(BOOL)enabled {
  BOOL changing = _enabled != enabled;
  if (changing) {
    if (!enabled) {
      [self dismissUI];
    }
    _enabled = enabled;
    [self enableCurrentWebState];
  }
}

- (void)cancelRequest {
  [self terminateRequestWithErrorMessage:@"The payment request was canceled."
                                callback:nil];
}

- (void)terminateRequestWithErrorMessage:(NSString*)errorMessage
                                callback:(ProceduralBlockWithBool)callback {
  [self dismissUI];
  [_paymentRequestJsManager rejectRequestPromiseWithErrorMessage:errorMessage
                                               completionHandler:callback];
}

- (void)close {
  if (_closed)
    return;

  _closed = YES;
  [self disableCurrentWebState];
  [self setWebState:nil];
  [self dismissUI];
}

- (void)enableCurrentWebState {
  if (![self webState]) {
    return;
  }

  if (_enabled) {
    if (!_webStateEnabled) {
      __weak PaymentRequestManager* weakSelf = self;
      auto callback = base::BindBlockArc(
          ^bool(const base::DictionaryValue& JSON, const GURL& originURL,
                bool userIsInteracting) {
            // |originURL| and |userIsInteracting| aren't used.
            return [weakSelf handleScriptCommand:JSON];
          });
      [self webState]->AddScriptCommandCallback(callback, kCommandPrefix);

      _webStateEnabled = YES;
    }
  } else {
    [self disableCurrentWebState];
  }
}

- (void)disableCurrentWebState {
  if (_webStateEnabled) {
    _webState->RemoveScriptCommandCallback(kCommandPrefix);
    _webStateEnabled = NO;
  }
}

- (void)disconnectWebState {
  if (_webState) {
    _paymentRequestJsManager = nil;
    _webStateObserver.reset();
    [self disableCurrentWebState];
  }
}

- (void)initializeWebViewForPaymentRequest {
  if (_enabled) {
    DCHECK(_webStateEnabled);

    [_paymentRequestJsManager inject];
    _isScriptInjected = YES;
  }
}

- (BOOL)handleScriptCommand:(const base::DictionaryValue&)JSONCommand {
  if (![self webStateContentIsSecureHTML]) {
    return NO;
  }

  std::string command;
  if (!JSONCommand.GetString("command", &command)) {
    DLOG(ERROR) << "RECEIVED BAD JSON - NO 'command' field";
    return NO;
  }

  if (command == "paymentRequest.requestShow") {
    return [self handleRequestShow:JSONCommand];
  }
  if (command == "paymentRequest.requestAbort") {
    return [self handleRequestAbort:JSONCommand];
  }
  if (command == "paymentRequest.responseComplete") {
    return [self handleResponseComplete:JSONCommand];
  }
  if (command == "paymentRequest.updatePaymentDetails") {
    return [self handleUpdatePaymentDetails:JSONCommand];
  }
  return NO;
}

- (BOOL)handleRequestShow:(const base::DictionaryValue&)message {
  // TODO(crbug.com/602666): check that there's not already a pending request.
  // TODO(crbug.com/602666): compare our supported payment types (i.e. autofill
  //   credit card types) against the merchant supported types and return NO
  //   if the intersection is empty.

  const base::DictionaryValue* paymentRequestData;
  web::PaymentRequest paymentRequest;
  if (!message.GetDictionary("payment_request", &paymentRequestData)) {
    DLOG(ERROR) << "JS message parameter 'payment_request' is missing";
    return NO;
  }
  if (!paymentRequest.FromDictionaryValue(*paymentRequestData)) {
    DLOG(ERROR) << "JS message parameter 'payment_request' is invalid";
    return NO;
  }

  _paymentRequest.reset(
      new PaymentRequest(base::MakeUnique<web::PaymentRequest>(paymentRequest),
                         _personalDataManager));

  UIImage* pageFavicon = nil;
  web::NavigationItem* navigationItem =
      [self webState]->GetNavigationManager()->GetVisibleItem();
  if (navigationItem && !navigationItem->GetFavicon().image.IsEmpty())
    pageFavicon = navigationItem->GetFavicon().image.ToUIImage();
  NSString* pageTitle = base::SysUTF16ToNSString([self webState]->GetTitle());
  NSString* pageHost =
      base::SysUTF8ToNSString([self webState]->GetLastCommittedURL().host());
  autofill::AutofillManager* autofillManager =
      autofill::AutofillDriverIOS::FromWebState(_webState)->autofill_manager();
  _paymentRequestCoordinator = [[PaymentRequestCoordinator alloc]
      initWithBaseViewController:_baseViewController];
  [_paymentRequestCoordinator setPaymentRequest:_paymentRequest.get()];
  [_paymentRequestCoordinator setAutofillManager:autofillManager];
  [_paymentRequestCoordinator setBrowserState:_browserState];
  [_paymentRequestCoordinator setPageFavicon:pageFavicon];
  [_paymentRequestCoordinator setPageTitle:pageTitle];
  [_paymentRequestCoordinator setPageHost:pageHost];
  [_paymentRequestCoordinator setDelegate:self];

  [_paymentRequestCoordinator start];

  return YES;
}

- (BOOL)handleRequestAbort:(const base::DictionaryValue&)message {
  // TODO(crbug.com/602666): Check that there is already a pending request.

  [_unblockEventQueueTimer invalidate];
  [_paymentResponseTimeoutTimer invalidate];
  [_updateEventTimeoutTimer invalidate];

  __weak PaymentRequestManager* weakSelf = self;

  ProceduralBlockWithBool cancellationCallback = ^(BOOL) {
    PaymentRequestManager* strongSelf = weakSelf;
    // Early return if the manager has been deallocated.
    if (!strongSelf)
      return;
    [[strongSelf paymentRequestJsManager]
        resolveAbortPromiseWithCompletionHandler:nil];
  };

  ProceduralBlock callback = ^{
    PaymentRequestManager* strongSelf = weakSelf;
    // Early return if the manager has been deallocated.
    if (!strongSelf)
      return;
    [strongSelf
        terminateRequestWithErrorMessage:@"The payment request was aborted."
                                callback:cancellationCallback];
  };

  [_paymentRequestCoordinator displayErrorWithCallback:callback];

  return YES;
}

- (BOOL)displayErrorThenCancelRequest {
  // TODO(crbug.com/602666): Check that there is already a pending request.

  [_unblockEventQueueTimer invalidate];
  [_paymentResponseTimeoutTimer invalidate];
  [_updateEventTimeoutTimer invalidate];

  __weak PaymentRequestManager* weakSelf = self;
  ProceduralBlock callback = ^{
    PaymentRequestManager* strongSelf = weakSelf;
    // Early return if the manager has been deallocated.
    if (!strongSelf)
      return;
    [strongSelf
        terminateRequestWithErrorMessage:@"The payment request was canceled."
                                callback:nil];
  };

  [_paymentRequestCoordinator displayErrorWithCallback:callback];

  return YES;
}

- (BOOL)doResponseComplete {
  base::DictionaryValue command;
  command.SetString("result", "unknown");
  return [self handleResponseComplete:command];
}

- (BOOL)handleResponseComplete:(const base::DictionaryValue&)message {
  // TODO(crbug.com/602666): Check that there *is* a pending response here.

  [_unblockEventQueueTimer invalidate];
  [_paymentResponseTimeoutTimer invalidate];
  [_updateEventTimeoutTimer invalidate];

  std::string result;
  if (!message.GetString("result", &result)) {
    DLOG(ERROR) << "JS message parameter 'result' is missing";
    return NO;
  }

  __weak PaymentRequestManager* weakSelf = self;
  ProceduralBlock callback = ^{
    PaymentRequestManager* strongSelf = weakSelf;
    // Early return if the manager has been deallocated.
    if (!strongSelf)
      return;
    [strongSelf dismissUI];
    [strongSelf.paymentRequestJsManager
        resolveResponsePromiseWithCompletionHandler:nil];
  };

  // Display UI indicating failure if the value of |result| is "fail".
  if (result == "fail") {
    [_paymentRequestCoordinator displayErrorWithCallback:callback];
  } else {
    callback();
  }

  return YES;
}

- (BOOL)handleUpdatePaymentDetails:(const base::DictionaryValue&)message {
  // TODO(crbug.com/602666): Check that there is already a pending request.

  [_unblockEventQueueTimer invalidate];
  [_updateEventTimeoutTimer invalidate];

  const base::DictionaryValue* paymentDetailsData = nullptr;
  web::PaymentDetails paymentDetails;
  if (!message.GetDictionary("payment_details", &paymentDetailsData)) {
    DLOG(ERROR) << "JS message parameter 'payment_details' is missing";
    return NO;
  }
  if (!paymentDetails.FromDictionaryValue(*paymentDetailsData)) {
    DLOG(ERROR) << "JS message parameter 'payment_details' is invalid";
    return NO;
  }

  [_paymentRequestCoordinator updatePaymentDetails:paymentDetails];

  return YES;
}

- (void)setUnblockEventQueueTimer {
  _unblockEventQueueTimer =
      [NSTimer scheduledTimerWithTimeInterval:kNoopInterval
                                       target:_paymentRequestJsManager
                                     selector:@selector(executeNoop)
                                     userInfo:nil
                                      repeats:YES];
}

- (void)setPaymentResponseTimeoutTimer {
  _paymentResponseTimeoutTimer =
      [NSTimer scheduledTimerWithTimeInterval:kTimeoutInterval
                                       target:self
                                     selector:@selector(doResponseComplete)
                                     userInfo:nil
                                      repeats:NO];
}

- (void)setUpdateEventTimeoutTimer {
  _updateEventTimeoutTimer = [NSTimer
      scheduledTimerWithTimeInterval:kTimeoutInterval
                              target:self
                            selector:@selector(displayErrorThenCancelRequest)
                            userInfo:nil
                             repeats:NO];
}

- (void)dismissUI {
  [_paymentRequestCoordinator stop];
  _paymentRequestCoordinator = nil;
}

- (BOOL)webStateContentIsSecureHTML {
  if (![self webState]) {
    return NO;
  }

  if (!web::UrlHasWebScheme([self webState]->GetLastCommittedURL()) ||
      ![self webState]->ContentIsHTML()) {
    return NO;
  }

  const web::NavigationItem* navigationItem =
      [self webState]->GetNavigationManager()->GetLastCommittedItem();
  return navigationItem &&
         navigationItem->GetSSL().security_style ==
             web::SECURITY_STYLE_AUTHENTICATED;
}

#pragma mark - PaymentRequestCoordinatorDelegate methods

- (void)paymentRequestCoordinatorDidCancel:
    (PaymentRequestCoordinator*)coordinator {
  [self terminateRequestWithErrorMessage:@"The payment request was canceled."
                                callback:nil];
}

- (void)paymentRequestCoordinator:(PaymentRequestCoordinator*)coordinator
    didConfirmWithPaymentResponse:(web::PaymentResponse)paymentResponse {
  [_paymentRequestJsManager
      resolveRequestPromiseWithPaymentResponse:paymentResponse
                             completionHandler:nil];
  [self setUnblockEventQueueTimer];
  [self setPaymentResponseTimeoutTimer];
}

- (void)paymentRequestCoordinator:(PaymentRequestCoordinator*)coordinator
         didSelectShippingAddress:(web::PaymentAddress)shippingAddress {
  [_paymentRequestJsManager updateShippingAddress:shippingAddress
                                completionHandler:nil];
  [self setUnblockEventQueueTimer];
  [self setUpdateEventTimeoutTimer];
}

- (void)paymentRequestCoordinator:(PaymentRequestCoordinator*)coordinator
          didSelectShippingOption:(web::PaymentShippingOption)shippingOption {
  [_paymentRequestJsManager updateShippingOption:shippingOption
                               completionHandler:nil];
  [self setUnblockEventQueueTimer];
  [self setUpdateEventTimeoutTimer];
}

#pragma mark - CRWWebStateObserver methods

- (void)webState:(web::WebState*)webState
    didCommitNavigationWithDetails:
        (const web::LoadCommittedDetails&)load_details {
  [self dismissUI];
  _isScriptInjected = NO;
  [self enableCurrentWebState];
  [self initializeWebViewForPaymentRequest];
}

@end
