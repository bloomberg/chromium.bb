// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/payment_request_manager.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>

#include "base/ios/block_types.h"
#include "base/ios/ios_util.h"
#include "base/json/json_reader.h"
#import "base/mac/bind_objc_block.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/values.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/ios/browser/autofill_driver_ios.h"
#include "components/payments/core/can_make_payment_query.h"
#include "components/payments/core/journey_logger.h"
#include "components/payments/core/payment_address.h"
#include "components/payments/core/payment_prefs.h"
#include "components/payments/core/payment_request_base_delegate.h"
#include "components/payments/core/payment_request_data_util.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/browser/autofill/validation_rules_storage_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/payments/ios_can_make_payment_query_factory.h"
#include "ios/chrome/browser/payments/ios_payment_request_cache_factory.h"
#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/payments/payment_request_cache.h"
#include "ios/chrome/browser/procedural_block_types.h"
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"
#import "ios/chrome/browser/ui/commands/ios_command_ids.h"
#import "ios/chrome/browser/ui/payments/js_payment_request_manager.h"
#import "ios/chrome/browser/ui/payments/payment_request_coordinator.h"
#include "ios/chrome/browser/ui/toolbar/toolbar_model_ios.h"
#include "ios/web/public/favicon_status.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/navigation_manager.h"
#include "ios/web/public/origin_util.h"
#include "ios/web/public/payments/payment_request.h"
#include "ios/web/public/ssl_status.h"
#import "ios/web/public/url_scheme_util.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#import "ios/web/public/web_state/ui/crw_web_view_proxy.h"
#include "ios/web/public/web_state/url_verification_constants.h"
#include "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"
#include "third_party/libaddressinput/chromium/chrome_metadata_source.h"
#include "third_party/libaddressinput/chromium/chrome_storage_impl.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Command prefix for injected JavaScript.
const char kCommandPrefix[] = "paymentRequest";

// Time interval between attempts to unblock the webview's JS event queue.
const NSTimeInterval kNoopInterval = 0.1;

// Time interval before closing the UI if the page has not yet called
// PaymentResponse.complete().
const NSTimeInterval kTimeoutInterval = 60.0;

NSString* kAbortMessage = @"The payment request was aborted.";
NSString* kCancelMessage = @"The payment request was canceled.";

struct PendingPaymentResponse {
  std::string methodName;
  std::string stringifiedDetails;
  autofill::AutofillProfile shippingAddress;
  autofill::AutofillProfile contactAddress;
};

}  // namespace

@interface PaymentRequestManager ()<CRWWebStateObserver,
                                    PaymentRequestCoordinatorDelegate,
                                    PaymentRequestUIDelegate> {
  // View controller used to present the PaymentRequest view controller.
  __weak UIViewController* _baseViewController;

  // PersonalDataManager used to manage user credit cards and addresses.
  autofill::PersonalDataManager* _personalDataManager;

  // Maintains a map of web::WebState to a list of payments::PaymentRequest
  // instances maintained for that WebState.
  payments::PaymentRequestCache* _paymentRequestCache;

  // The observer for |_activeWebState|.
  std::unique_ptr<web::WebStateObserverBridge> _activeWebStateObserver;

  // Boolean to track if the active WebState is enabled (JS callback is set
  // up).
  BOOL _activeWebStateEnabled;

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

  // Storage for data to return in the payment response, until we're ready to
  // send an actual PaymentResponse.
  PendingPaymentResponse _pendingPaymentResponse;
}

// YES if Payment Request is enabled on the active web state.
@property(readonly) BOOL enabled;

// The ios::ChromeBrowserState instance passed to the initializer.
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;

// Object that manages JavaScript injection into the web view.
@property(nonatomic, weak) JSPaymentRequestManager* paymentRequestJsManager;

// The payments::PaymentRequest instance currently showing, if any.
@property(nonatomic, assign) payments::PaymentRequest* pendingPaymentRequest;

// Synchronous method executed by -asynchronouslyEnablePaymentRequest:
- (void)doEnablePaymentRequest:(BOOL)enabled;

// Terminates the pending request with an error message and dismisses the UI.
// Invokes the callback once the request has been terminated.
- (void)terminatePendingRequestWithErrorMessage:(NSString*)errorMessage
                                       callback:
                                           (ProceduralBlockWithBool)callback;

// Handler for injected JavaScript callbacks.
- (BOOL)handleScriptCommand:(const base::DictionaryValue&)JSONCommand;

// Handles creation of a PaymentRequest instance. The value of the JavaScript
// PaymentRequest object should be provided in |message|. Returns YES if the
// invocation was successful.
- (BOOL)handleCreatePaymentRequest:(const base::DictionaryValue&)message;

// Handles invocations of PaymentRequest.show(). The value of the JavaScript
// PaymentRequest object should be provided in |message|. Returns YES if the
// invocation was successful.
- (BOOL)handleRequestShow:(const base::DictionaryValue&)message;

// Handles invocations of PaymentRequest.abort(). Returns YES if the invocation
// was successful.
- (BOOL)handleRequestAbort:(const base::DictionaryValue&)message;

// Handles invocations of PaymentRequest.canMakePayment(). Returns YES if the
// invocation was successful.
- (BOOL)handleCanMakePayment:(const base::DictionaryValue&)message;

// Called by |_updateEventTimeoutTimer|, displays an error message. Upon
// dismissal of the error message, cancels the Payment Request as if it was
// performed by the user.
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

// Called when the relevant addresses from a Payment Request have been
// normalized. Resolves the request promise with a PaymentResponse.
- (void)paymentRequestAddressNormalizationDidCompleteForPaymentRequest:
    (payments::PaymentRequest*)paymentRequest;

// Returns the instance of payments::PaymentRequest for self.activeWebState that
// has the identifier |paymentRequestId|, if any. Otherwise returns nullptr.
- (payments::PaymentRequest*)paymentRequestWithId:(std::string)paymentRequestId;

@end

@implementation PaymentRequestManager

@synthesize toolbarModel = _toolbarModel;
@synthesize browserState = _browserState;
@synthesize enabled = _enabled;
@synthesize activeWebState = _activeWebState;
@synthesize paymentRequestJsManager = _paymentRequestJsManager;
@synthesize pendingPaymentRequest = _pendingPaymentRequest;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState {
  if ((self = [super init])) {
    _baseViewController = viewController;

    _browserState = browserState;

    _personalDataManager =
        autofill::PersonalDataManagerFactory::GetForBrowserState(
            browserState->GetOriginalChromeBrowserState());

    _paymentRequestCache =
        payments::IOSPaymentRequestCacheFactory::GetForBrowserState(
            browserState->GetOriginalChromeBrowserState());
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (void)setActiveWebState:(web::WebState*)webState {
  // First cancel any pending request.
  [self cancelRequest];
  [self disconnectActiveWebState];
  if (webState) {
    _paymentRequestJsManager =
        base::mac::ObjCCastStrict<JSPaymentRequestManager>(
            [webState->GetJSInjectionReceiver()
                instanceOfClass:[JSPaymentRequestManager class]]);
    _activeWebState = webState;
    _activeWebStateObserver =
        base::MakeUnique<web::WebStateObserverBridge>(webState, self);
    [self enableActiveWebState];
  } else {
    _activeWebState = nullptr;
  }
}

- (void)stopTrackingWebState:(web::WebState*)webState {
  // The lifetime of a PaymentRequest is tied to the WebState it is associated
  // with and the current URL. Therefore, PaymentRequest instances should get
  // destroyed when the WebState goes away.
  _paymentRequestCache->ClearPaymentRequests(webState);
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
    [self enableActiveWebState];
  }
}

- (void)cancelRequest {
  if (!_pendingPaymentRequest)
    return;
  _pendingPaymentRequest->journey_logger().SetAborted(
      payments::JourneyLogger::ABORT_REASON_MERCHANT_NAVIGATION);

  [self terminatePendingRequestWithErrorMessage:kCancelMessage callback:nil];
}

- (void)terminatePendingRequestWithErrorMessage:(NSString*)errorMessage
                                       callback:
                                           (ProceduralBlockWithBool)callback {
  DCHECK(_pendingPaymentRequest);
  _pendingPaymentRequest = nullptr;
  [self dismissUI];
  [_paymentRequestJsManager rejectRequestPromiseWithErrorMessage:errorMessage
                                               completionHandler:callback];
}

- (void)close {
  if (_closed)
    return;

  _closed = YES;
  [self disableActiveWebState];
  [self setActiveWebState:nil];
  [self dismissUI];
}

- (void)enableActiveWebState {
  if (!_activeWebState) {
    return;
  }

  if (_enabled) {
    if (!_activeWebStateEnabled) {
      __weak PaymentRequestManager* weakSelf = self;
      auto callback = base::BindBlockArc(
          ^bool(const base::DictionaryValue& JSON, const GURL& originURL,
                bool userIsInteracting) {
            // |originURL| and |userIsInteracting| aren't used.
            return [weakSelf handleScriptCommand:JSON];
          });
      _activeWebState->AddScriptCommandCallback(callback, kCommandPrefix);

      _activeWebStateEnabled = YES;
    }
  } else {
    [self disableActiveWebState];
  }
}

- (void)disableActiveWebState {
  if (_activeWebState && _activeWebStateEnabled) {
    _activeWebState->RemoveScriptCommandCallback(kCommandPrefix);
    _activeWebStateEnabled = NO;
  }
}

- (void)disconnectActiveWebState {
  if (_activeWebState) {
    _paymentRequestJsManager = nil;
    _activeWebStateObserver.reset();
    [self disableActiveWebState];
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

  if (command == "paymentRequest.createPaymentRequest") {
    return [self handleCreatePaymentRequest:JSONCommand];
  }
  if (command == "paymentRequest.requestShow") {
    return [self handleRequestShow:JSONCommand];
  }
  if (command == "paymentRequest.requestAbort") {
    return [self handleRequestAbort:JSONCommand];
  }
  if (command == "paymentRequest.requestCanMakePayment") {
    return [self handleCanMakePayment:JSONCommand];
  }
  if (command == "paymentRequest.responseComplete") {
    return [self handleResponseComplete:JSONCommand];
  }
  if (command == "paymentRequest.updatePaymentDetails") {
    return [self handleUpdatePaymentDetails:JSONCommand];
  }
  return NO;
}

// Extracts a web::PaymentRequest from |message|. Creates and returns an
// instance of payments::PaymentRequest which is initialized with the
// web::PaymentRequest object. Returns nullptr if it cannot extract a
// web::PaymentRequest from |message|.
- (payments::PaymentRequest*)newPaymentRequestFromMessage:
    (const base::DictionaryValue&)message {
  const base::DictionaryValue* paymentRequestData;
  web::PaymentRequest webPaymentRequest;
  if (!message.GetDictionary("payment_request", &paymentRequestData)) {
    DLOG(ERROR) << "JS message parameter 'payment_request' is missing";
    return nullptr;
  }
  if (!webPaymentRequest.FromDictionaryValue(*paymentRequestData)) {
    DLOG(ERROR) << "JS message parameter 'payment_request' is invalid";
    return nullptr;
  }
  return _paymentRequestCache->AddPaymentRequest(
      _activeWebState, base::MakeUnique<payments::PaymentRequest>(
                           webPaymentRequest, _browserState, _activeWebState,
                           _personalDataManager, self));
}

// Extracts a web::PaymentRequest from |message|. Returns the cached instance of
// payments::PaymentRequest that corresponds to the extracted
// web::PaymentRequest object, if one exists. Otherwise, creates and returns a
// new one which is initialized with the web::PaymentRequest object. Returns
// nullptr if it cannot extract a web::PaymentRequest from |message| or cannot
// find the payments::PaymentRequest instance.
- (payments::PaymentRequest*)paymentRequestFromMessage:
    (const base::DictionaryValue&)message {
  const base::DictionaryValue* paymentRequestData;
  web::PaymentRequest webPaymentRequest;
  if (!message.GetDictionary("payment_request", &paymentRequestData)) {
    DLOG(ERROR) << "JS message parameter 'payment_request' is missing";
    return nullptr;
  }
  if (!webPaymentRequest.FromDictionaryValue(*paymentRequestData)) {
    DLOG(ERROR) << "JS message parameter 'payment_request' is invalid";
    return nullptr;
  }

  return [self paymentRequestWithId:webPaymentRequest.payment_request_id];
}

- (BOOL)handleCreatePaymentRequest:(const base::DictionaryValue&)message {
  payments::PaymentRequest* paymentRequest =
      [self newPaymentRequestFromMessage:message];
  if (!paymentRequest) {
    // TODO(crbug.com/602666): Reject the promise with an error of
    // "InvalidStateError" type.
    [_paymentRequestJsManager
        rejectCanMakePaymentPromiseWithErrorMessage:@"Invalid state error"
                                  completionHandler:nil];
  }
  return YES;
}

- (BOOL)handleRequestShow:(const base::DictionaryValue&)message {
  payments::PaymentRequest* paymentRequest =
      [self paymentRequestFromMessage:message];
  if (!paymentRequest) {
    // TODO(crbug.com/602666): Reject the promise with an error of
    // "InvalidStateError" type.
    [_paymentRequestJsManager
        rejectRequestPromiseWithErrorMessage:@"Invalid state error"
                           completionHandler:nil];
    return YES;
  }

  if (_pendingPaymentRequest) {
    paymentRequest->journey_logger().SetNotShown(
        payments::JourneyLogger::NOT_SHOWN_REASON_CONCURRENT_REQUESTS);
    // TODO(crbug.com/602666): Reject the promise with an error of
    // "InvalidStateError" type.
    [_paymentRequestJsManager
        rejectRequestPromiseWithErrorMessage:@"Invalid state error"
                           completionHandler:nil];
    return YES;
  }

  if (paymentRequest->supported_card_networks().empty() &&
      paymentRequest->url_payment_method_identifiers().empty()) {
    paymentRequest->journey_logger().SetNotShown(
        payments::JourneyLogger::NOT_SHOWN_REASON_NO_SUPPORTED_PAYMENT_METHOD);
    // TODO(crbug.com/602666): Reject the promise with an error of
    // "InvalidStateError" type.
    [_paymentRequestJsManager
        rejectRequestPromiseWithErrorMessage:@"Invalid state error"
                           completionHandler:nil];
    return YES;
  }

  _pendingPaymentRequest = paymentRequest;

  paymentRequest->journey_logger().SetShowCalled();
  paymentRequest->journey_logger().SetEventOccurred(
      payments::JourneyLogger::EVENT_SHOWN);
  paymentRequest->journey_logger().SetRequestedInformation(
      paymentRequest->request_shipping(), paymentRequest->request_payer_email(),
      paymentRequest->request_payer_phone(),
      paymentRequest->request_payer_name());

  UIImage* pageFavicon = nil;
  web::NavigationItem* navigationItem =
      _activeWebState->GetNavigationManager()->GetVisibleItem();
  if (navigationItem && !navigationItem->GetFavicon().image.IsEmpty())
    pageFavicon = navigationItem->GetFavicon().image.ToUIImage();
  NSString* pageTitle = base::SysUTF16ToNSString(_activeWebState->GetTitle());
  NSString* pageHost =
      base::SysUTF8ToNSString(_activeWebState->GetLastCommittedURL().host());
  BOOL connectionSecure =
      _activeWebState->GetLastCommittedURL().SchemeIs(url::kHttpsScheme);
  autofill::AutofillManager* autofillManager =
      autofill::AutofillDriverIOS::FromWebState(_activeWebState)
          ->autofill_manager();
  _paymentRequestCoordinator = [[PaymentRequestCoordinator alloc]
      initWithBaseViewController:_baseViewController];
  [_paymentRequestCoordinator setPaymentRequest:paymentRequest];
  [_paymentRequestCoordinator setAutofillManager:autofillManager];
  [_paymentRequestCoordinator setBrowserState:_browserState];
  [_paymentRequestCoordinator setPageFavicon:pageFavicon];
  [_paymentRequestCoordinator setPageTitle:pageTitle];
  [_paymentRequestCoordinator setPageHost:pageHost];
  [_paymentRequestCoordinator setConnectionSecure:connectionSecure];
  [_paymentRequestCoordinator setDelegate:self];

  [_paymentRequestCoordinator start];

  return YES;
}

- (BOOL)handleRequestAbort:(const base::DictionaryValue&)message {
  DCHECK(_pendingPaymentRequest);

  _pendingPaymentRequest->journey_logger().SetAborted(
      payments::JourneyLogger::ABORT_REASON_ABORTED_BY_MERCHANT);

  [_unblockEventQueueTimer invalidate];
  [_paymentResponseTimeoutTimer invalidate];
  [_updateEventTimeoutTimer invalidate];

  __weak PaymentRequestManager* weakSelf = self;

  ProceduralBlockWithBool cancellationCallback = ^(BOOL) {
    [[weakSelf paymentRequestJsManager]
        resolveAbortPromiseWithCompletionHandler:nil];
  };

  ProceduralBlock callback = ^{
    [weakSelf terminatePendingRequestWithErrorMessage:kAbortMessage
                                             callback:cancellationCallback];
  };

  [_paymentRequestCoordinator displayErrorWithCallback:callback];

  return YES;
}

- (BOOL)handleCanMakePayment:(const base::DictionaryValue&)message {
  payments::PaymentRequest* paymentRequest =
      [self paymentRequestFromMessage:message];
  if (!paymentRequest) {
    // TODO(crbug.com/602666): Reject the promise with an error of
    // "InvalidStateError" type.
    [_paymentRequestJsManager
        rejectCanMakePaymentPromiseWithErrorMessage:@"Invalid state error"
                                  completionHandler:nil];
    return YES;
  }

  if (paymentRequest->IsIncognito()) {
    [_paymentRequestJsManager resolveCanMakePaymentPromiseWithValue:YES
                                                  completionHandler:nil];
    paymentRequest->journey_logger().SetCanMakePaymentValue(true);
    return YES;
  }

  BOOL canMakePayment = paymentRequest->CanMakePayment();

  payments::CanMakePaymentQuery* canMakePaymentQuery =
      IOSCanMakePaymentQueryFactory::GetInstance()->GetForBrowserState(
          _browserState);
  DCHECK(canMakePaymentQuery);
  if (canMakePaymentQuery->CanQuery(
          _activeWebState->GetLastCommittedURL().GetOrigin(),
          paymentRequest->stringified_method_data())) {
    [_paymentRequestJsManager
        resolveCanMakePaymentPromiseWithValue:canMakePayment
                            completionHandler:nil];
    paymentRequest->journey_logger().SetCanMakePaymentValue(canMakePayment);
    // TODO(crbug.com/602666): Warn on console if origin is localhost or file.
  } else {
    [_paymentRequestJsManager
        rejectCanMakePaymentPromiseWithErrorMessage:
            @"Not allowed to check whether can make payment"
                                  completionHandler:nil];
  }
  return YES;
}

- (BOOL)displayErrorThenCancelRequest {
  DCHECK(_pendingPaymentRequest);
  _pendingPaymentRequest->journey_logger().SetAborted(
      payments::JourneyLogger::ABORT_REASON_ABORTED_BY_USER);

  [_unblockEventQueueTimer invalidate];
  [_paymentResponseTimeoutTimer invalidate];
  [_updateEventTimeoutTimer invalidate];

  __weak PaymentRequestManager* weakSelf = self;
  ProceduralBlock callback = ^{
    [weakSelf terminatePendingRequestWithErrorMessage:kCancelMessage
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
  DCHECK(_pendingPaymentRequest);

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
    weakSelf.pendingPaymentRequest = nullptr;
    [weakSelf dismissUI];
    [weakSelf.paymentRequestJsManager
        resolveResponsePromiseWithCompletionHandler:nil];
  };

  // Display UI indicating failure if the value of |result| is "fail".
  if (result == "fail") {
    [_paymentRequestCoordinator displayErrorWithCallback:callback];
  } else {
    _pendingPaymentRequest->journey_logger().SetCompleted();
    _pendingPaymentRequest->RecordUseStats();
    _pendingPaymentRequest->GetPrefService()->SetBoolean(
        payments::kPaymentsFirstTransactionCompleted, true);
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
  if (!paymentDetails.FromDictionaryValue(*paymentDetailsData,
                                          /*requires_total=*/false)) {
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
  if (!_activeWebState) {
    return NO;
  }

  if (!_toolbarModel) {
    return NO;
  }

  // Checks if the current page is a web view with HTML and that the
  // origin is localhost, file://, or cryptographic.
  if (!web::IsOriginSecure(_activeWebState->GetLastCommittedURL()) ||
      !_activeWebState->ContentIsHTML()) {
    return NO;
  }

  if (!_activeWebState->GetLastCommittedURL().SchemeIsCryptographic()) {
    // The URL has a secure origin, but is not https, so it must be local.
    // Return YES at this point, because localhost and filesystem URLS are
    // considered secure regardless of scheme.
    return YES;
  }

  // The following security level checks ensure that if the scheme is
  // cryptographic then the SSL certificate is valid.
  security_state::SecurityLevel securityLevel =
      _toolbarModel->GetToolbarModel()->GetSecurityLevel(true);
  return securityLevel == security_state::EV_SECURE ||
         securityLevel == security_state::SECURE ||
         securityLevel == security_state::SECURE_WITH_POLICY_INSTALLED_CERT;
}

#pragma mark - PaymentRequestUIDelegate

- (void)
requestFullCreditCard:(const autofill::CreditCard&)creditCard
       resultDelegate:
           (base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>)
               resultDelegate {
  [_paymentRequestCoordinator requestFullCreditCard:creditCard
                                     resultDelegate:resultDelegate];
}

#pragma mark - PaymentRequestCoordinatorDelegate methods

- (void)paymentRequestCoordinatorDidCancel:
    (PaymentRequestCoordinator*)coordinator {
  coordinator.paymentRequest->journey_logger().SetAborted(
      payments::JourneyLogger::ABORT_REASON_ABORTED_BY_USER);

  [self terminatePendingRequestWithErrorMessage:kCancelMessage callback:nil];
}

- (void)paymentRequestCoordinatorDidSelectSettings:
    (PaymentRequestCoordinator*)coordinator {
  coordinator.paymentRequest->journey_logger().SetAborted(
      payments::JourneyLogger::ABORT_REASON_ABORTED_BY_USER);

  ProceduralBlockWithBool callback = ^(BOOL) {
    UIWindow* mainWindow = [[UIApplication sharedApplication] keyWindow];
    DCHECK(mainWindow);
    GenericChromeCommand* command =
        [[GenericChromeCommand alloc] initWithTag:IDC_SHOW_AUTOFILL_SETTINGS];
    [mainWindow chromeExecuteCommand:command];
  };

  [self terminatePendingRequestWithErrorMessage:kCancelMessage
                                       callback:callback];
}

- (void)paymentRequestCoordinator:(PaymentRequestCoordinator*)coordinator
         didReceiveFullMethodName:(const std::string&)methodName
               stringifiedDetails:(const std::string&)stringifiedDetails {
  coordinator.paymentRequest->journey_logger().SetEventOccurred(
      payments::JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);

  _pendingPaymentResponse.methodName = methodName;
  _pendingPaymentResponse.stringifiedDetails = stringifiedDetails;

  if (coordinator.paymentRequest->request_shipping()) {
    // TODO(crbug.com/602666): User should get here only if they have selected
    // a shipping address.
    DCHECK(coordinator.paymentRequest->selected_shipping_profile());
    _pendingPaymentResponse.shippingAddress =
        *coordinator.paymentRequest->selected_shipping_profile();
    coordinator.paymentRequest->address_normalization_manager()
        ->StartNormalizingAddress(&_pendingPaymentResponse.shippingAddress);
  }

  if (coordinator.paymentRequest->request_payer_name() ||
      coordinator.paymentRequest->request_payer_email() ||
      coordinator.paymentRequest->request_payer_phone()) {
    // TODO(crbug.com/602666): User should get here only if they have selected
    // a contact info.
    DCHECK(coordinator.paymentRequest->selected_contact_profile());
    _pendingPaymentResponse.contactAddress =
        *coordinator.paymentRequest->selected_contact_profile();
    coordinator.paymentRequest->address_normalization_manager()
        ->StartNormalizingAddress(&_pendingPaymentResponse.contactAddress);
  }

  __weak PaymentRequestManager* weakSelf = self;
  __weak PaymentRequestCoordinator* weakCoordinator = coordinator;
  coordinator.paymentRequest->address_normalization_manager()
      ->FinalizePendingRequestsWithCompletionCallback(base::BindBlockArc(^() {
        [weakSelf
            paymentRequestAddressNormalizationDidCompleteForPaymentRequest:
                weakCoordinator.paymentRequest];
      }));
}

- (void)paymentRequestAddressNormalizationDidCompleteForPaymentRequest:
    (payments::PaymentRequest*)paymentRequest {
  web::PaymentResponse paymentResponse;

  paymentResponse.payment_request_id =
      paymentRequest->web_payment_request().payment_request_id;

  paymentResponse.method_name =
      base::ASCIIToUTF16(_pendingPaymentResponse.methodName);

  paymentResponse.details = _pendingPaymentResponse.stringifiedDetails;

  if (paymentRequest->request_shipping()) {
    paymentResponse.shipping_address =
        payments::data_util::GetPaymentAddressFromAutofillProfile(
            _pendingPaymentResponse.shippingAddress,
            paymentRequest->GetApplicationLocale());

    web::PaymentShippingOption* shippingOption =
        paymentRequest->selected_shipping_option();
    DCHECK(shippingOption);
    paymentResponse.shipping_option = shippingOption->id;
  }

  if (paymentRequest->request_payer_name()) {
    paymentResponse.payer_name = _pendingPaymentResponse.contactAddress.GetInfo(
        autofill::AutofillType(autofill::NAME_FULL),
        paymentRequest->GetApplicationLocale());
  }

  if (paymentRequest->request_payer_email()) {
    paymentResponse.payer_email =
        _pendingPaymentResponse.contactAddress.GetRawInfo(
            autofill::EMAIL_ADDRESS);
  }

  if (paymentRequest->request_payer_phone()) {
    paymentResponse.payer_phone =
        _pendingPaymentResponse.contactAddress.GetRawInfo(
            autofill::PHONE_HOME_WHOLE_NUMBER);
  }

  [_paymentRequestJsManager
      resolveRequestPromiseWithPaymentResponse:paymentResponse
                             completionHandler:nil];
  [self setUnblockEventQueueTimer];
  [self setPaymentResponseTimeoutTimer];
}

- (void)paymentRequestCoordinator:(PaymentRequestCoordinator*)coordinator
         didSelectShippingAddress:
             (const autofill::AutofillProfile&)shippingAddress {
  payments::PaymentAddress address =
      payments::data_util::GetPaymentAddressFromAutofillProfile(
          shippingAddress, coordinator.paymentRequest->GetApplicationLocale());
  [_paymentRequestJsManager updateShippingAddress:address
                                completionHandler:nil];
  [self setUnblockEventQueueTimer];
  [self setUpdateEventTimeoutTimer];
}

- (void)paymentRequestCoordinator:(PaymentRequestCoordinator*)coordinator
          didSelectShippingOption:
              (const web::PaymentShippingOption&)shippingOption {
  [_paymentRequestJsManager updateShippingOption:shippingOption
                               completionHandler:nil];
  [self setUnblockEventQueueTimer];
  [self setUpdateEventTimeoutTimer];
}

#pragma mark - CRWWebStateObserver methods

- (void)webState:(web::WebState*)webState
    didCommitNavigationWithDetails:
        (const web::LoadCommittedDetails&)load_details {
  // Reset any pending request.
  if (_pendingPaymentRequest) {
    _pendingPaymentRequest->journey_logger().SetAborted(
        payments::JourneyLogger::ABORT_REASON_MERCHANT_NAVIGATION);
    _pendingPaymentRequest = nullptr;
  }

  [self dismissUI];
  [self enableActiveWebState];

  // The lifetime of a PaymentRequest is tied to the WebState it is associated
  // with and the current URL. Therefore, PaymentRequest instances should get
  // destroyed when the WebState goes away or the user navigates to a URL.
  _paymentRequestCache->ClearPaymentRequests(_activeWebState);
}

#pragma mark - Helper methods

- (payments::PaymentRequest*)paymentRequestWithId:
    (std::string)paymentRequestId {
  const payments::PaymentRequestCache::PaymentRequestSet& paymentRequests =
      _paymentRequestCache->GetPaymentRequests(_activeWebState);
  const auto found = std::find_if(
      paymentRequests.begin(), paymentRequests.end(),
      [&paymentRequestId](
          const std::unique_ptr<payments::PaymentRequest>& request) {
        return request.get()->web_payment_request().payment_request_id ==
               paymentRequestId;
      });
  return found != paymentRequests.end() ? found->get() : nullptr;
}

@end
