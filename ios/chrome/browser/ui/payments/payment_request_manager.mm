// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/payment_request_manager.h"

#include "base/ios/block_types.h"
#include "base/ios/ios_util.h"
#import "base/mac/bind_objc_block.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/values.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/ios/browser/autofill_driver_ios.h"
#include "components/payments/core/address_normalization_manager.h"
#include "components/payments/core/address_normalizer_impl.h"
#include "components/payments/core/payment_address.h"
#include "components/payments/core/payment_request_data_util.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/browser/autofill/validation_rules_storage_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/payments/payment_request.h"
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
  autofill::CreditCard creditCard;
  base::string16 verificationCode;
  autofill::AutofillProfile billingAddress;
  autofill::AutofillProfile shippingAddress;
  autofill::AutofillProfile contactAddress;
};

}  // namespace

@interface PaymentRequestManager ()<CRWWebStateObserver,
                                    PaymentRequestCoordinatorDelegate> {
  // View controller used to present the PaymentRequest view controller.
  __weak UIViewController* _baseViewController;

  // PersonalDataManager used to manage user credit cards and addresses.
  autofill::PersonalDataManager* _personalDataManager;

  // Object that has a copy of web::PaymentRequest as provided by the page
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

  // AddressNormalizationManager used to normalize the various addresses (e.g.
  // shipping, contact, billing).
  std::unique_ptr<payments::AddressNormalizationManager>
      _addressNormalizationManager;

  // Storage for data to return in the payment response, until we're ready to
  // send an actual PaymentResponse.
  PendingPaymentResponse _pendingPaymentResponse;
}

// Object that manages JavaScript injection into the web view.
@property(nonatomic, weak) JSPaymentRequestManager* paymentRequestJsManager;

// Synchronous method executed by -asynchronouslyEnablePaymentRequest:
- (void)doEnablePaymentRequest:(BOOL)enabled;

// Terminates the pending request with an error message and dismisses the UI.
// Invokes the callback once the request has been terminated.
- (void)terminateRequestWithErrorMessage:(NSString*)errorMessage
                                callback:(ProceduralBlockWithBool)callback;

// Handler for injected JavaScript callbacks.
- (BOOL)handleScriptCommand:(const base::DictionaryValue&)JSONCommand;

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
- (void)paymentRequestAddressNormalizationDidComplete;

@end

@implementation PaymentRequestManager

@synthesize enabled = _enabled;
@synthesize toolbarModel = _toolbarModel;
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
  [self terminateRequestWithErrorMessage:kCancelMessage callback:nil];
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
  if (_webState && _webStateEnabled) {
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

- (void)startAddressNormalizer {
  autofill::PersonalDataManager* personalDataManager =
      _paymentRequest->GetPersonalDataManager();

  std::unique_ptr<i18n::addressinput::Source> addressNormalizerSource =
      base::MakeUnique<autofill::ChromeMetadataSource>(
          I18N_ADDRESS_VALIDATION_DATA_URL,
          personalDataManager->GetURLRequestContextGetter());

  std::unique_ptr<i18n::addressinput::Storage> addressNormalizerStorage =
      autofill::ValidationRulesStorageFactory::CreateStorage();

  std::unique_ptr<payments::AddressNormalizer> addressNormalizer =
      base::MakeUnique<payments::AddressNormalizerImpl>(
          std::move(addressNormalizerSource),
          std::move(addressNormalizerStorage));

  // Kickoff the process of loading the rules (which is asynchronous) for each
  // profile's country, to get faster address normalization later.
  for (const autofill::AutofillProfile* profile :
       personalDataManager->GetProfilesToSuggest()) {
    std::string countryCode =
        base::UTF16ToUTF8(profile->GetRawInfo(autofill::ADDRESS_HOME_COUNTRY));
    if (autofill::data_util::IsValidCountryCode(countryCode)) {
      addressNormalizer->LoadRulesForRegion(countryCode);
    }
  }

  const std::string default_country_code =
      autofill::AutofillCountry::CountryCodeForLocale(
          GetApplicationContext()->GetApplicationLocale());

  _addressNormalizationManager =
      base::MakeUnique<payments::AddressNormalizationManager>(
          std::move(addressNormalizer), default_country_code);
}

// Ensures that |_paymentRequest| is set to the correct value for |message|.
// Returns YES if |_paymentRequest| was already set to the right value, or if it
// was updated to match |message|.
- (BOOL)createPaymentRequestFromMessage:(const base::DictionaryValue&)message {
  const base::DictionaryValue* paymentRequestData;
  web::PaymentRequest webPaymentRequest;
  if (!message.GetDictionary("payment_request", &paymentRequestData)) {
    DLOG(ERROR) << "JS message parameter 'payment_request' is missing";
    return NO;
  }
  if (!webPaymentRequest.FromDictionaryValue(*paymentRequestData)) {
    DLOG(ERROR) << "JS message parameter 'payment_request' is invalid";
    return NO;
  }

  // TODO(crbug.com/711419): make sure multiple PaymentRequests can be active
  // simultaneously.
  if (_paymentRequest &&
      (_paymentRequest->web_payment_request() == webPaymentRequest)) {
    return YES;
  }

  _paymentRequest =
      base::MakeUnique<PaymentRequest>(webPaymentRequest, _personalDataManager);
  return YES;
}

- (BOOL)handleRequestShow:(const base::DictionaryValue&)message {
  // TODO(crbug.com/602666): check that there's not already a pending request.
  // TODO(crbug.com/602666): compare our supported payment types (i.e. autofill
  //   credit card types) against the merchant supported types and return NO
  //   if the intersection is empty.

  if (![self createPaymentRequestFromMessage:message]) {
    return NO;
  }

  [self startAddressNormalizer];

  UIImage* pageFavicon = nil;
  web::NavigationItem* navigationItem =
      [self webState]->GetNavigationManager()->GetVisibleItem();
  if (navigationItem && !navigationItem->GetFavicon().image.IsEmpty())
    pageFavicon = navigationItem->GetFavicon().image.ToUIImage();
  NSString* pageTitle = base::SysUTF16ToNSString([self webState]->GetTitle());
  NSString* pageHost =
      base::SysUTF8ToNSString([self webState]->GetLastCommittedURL().host());
  BOOL connectionSecure =
      [self webState]->GetLastCommittedURL().SchemeIs(url::kHttpsScheme);
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
  [_paymentRequestCoordinator setConnectionSecure:connectionSecure];
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
    [[weakSelf paymentRequestJsManager]
        resolveAbortPromiseWithCompletionHandler:nil];
  };

  ProceduralBlock callback = ^{
    [weakSelf terminateRequestWithErrorMessage:kAbortMessage
                                      callback:cancellationCallback];
  };

  [_paymentRequestCoordinator displayErrorWithCallback:callback];

  return YES;
}

- (BOOL)handleCanMakePayment:(const base::DictionaryValue&)message {
  if (![self createPaymentRequestFromMessage:message]) {
    return NO;
  }

  // TODO(crbug.com/602666): reject the promise if quota (TBD) was exceeded.

  [_paymentRequestJsManager
      resolveCanMakePaymentPromiseWithValue:_paymentRequest->CanMakePayment()
                          completionHandler:nil];

  return YES;
}

- (BOOL)displayErrorThenCancelRequest {
  // TODO(crbug.com/602666): Check that there is already a pending request.

  [_unblockEventQueueTimer invalidate];
  [_paymentResponseTimeoutTimer invalidate];
  [_updateEventTimeoutTimer invalidate];

  __weak PaymentRequestManager* weakSelf = self;
  ProceduralBlock callback = ^{
    [weakSelf terminateRequestWithErrorMessage:kCancelMessage callback:nil];
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
    [weakSelf dismissUI];
    [weakSelf.paymentRequestJsManager
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

  if (![self toolbarModel]) {
    return NO;
  }

  // Checks if the current page is a web view with HTML and that the
  // origin is localhost, file://, or cryptographic.
  if (!web::IsOriginSecure([self webState]->GetLastCommittedURL()) ||
      ![self webState]->ContentIsHTML()) {
    return NO;
  }

  if (![self webState]->GetLastCommittedURL().SchemeIsCryptographic()) {
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

#pragma mark - PaymentRequestCoordinatorDelegate methods

- (void)paymentRequestCoordinatorDidCancel:
    (PaymentRequestCoordinator*)coordinator {
  [self terminateRequestWithErrorMessage:kCancelMessage callback:nil];
}

- (void)paymentRequestCoordinatorDidSelectSettings:
    (PaymentRequestCoordinator*)coordinator {
  ProceduralBlockWithBool callback = ^(BOOL) {
    UIWindow* mainWindow = [[UIApplication sharedApplication] keyWindow];
    DCHECK(mainWindow);
    GenericChromeCommand* command =
        [[GenericChromeCommand alloc] initWithTag:IDC_SHOW_AUTOFILL_SETTINGS];
    [mainWindow chromeExecuteCommand:command];
  };

  [self terminateRequestWithErrorMessage:kCancelMessage callback:callback];
}

- (void)paymentRequestCoordinator:(PaymentRequestCoordinator*)coordinator
    didCompletePaymentRequestWithCard:(const autofill::CreditCard&)card
                     verificationCode:(const base::string16&)verificationCode {
  _pendingPaymentResponse.creditCard = card;
  _pendingPaymentResponse.verificationCode = verificationCode;

  // TODO(crbug.com/714768): Make sure the billing address is set and valid
  // before getting here. Once the bug is addressed, there will be no need to
  // copy the address, *billing_address_ptr can be used to get the basic card
  // response.
  if (!card.billing_address_id().empty()) {
    autofill::AutofillProfile* billingAddressPtr =
        autofill::PersonalDataManager::GetProfileFromProfilesByGUID(
            card.billing_address_id(), _paymentRequest->billing_profiles());
    if (billingAddressPtr) {
      _pendingPaymentResponse.billingAddress = *billingAddressPtr;
      _addressNormalizationManager->StartNormalizingAddress(
          &_pendingPaymentResponse.billingAddress);
    }
  }

  if (_paymentRequest->request_shipping()) {
    // TODO(crbug.com/602666): User should get here only if they have selected
    // a shipping address.
    DCHECK(_paymentRequest->selected_shipping_profile());
    _pendingPaymentResponse.shippingAddress =
        *_paymentRequest->selected_shipping_profile();
    _addressNormalizationManager->StartNormalizingAddress(
        &_pendingPaymentResponse.shippingAddress);
  }

  if (_paymentRequest->request_payer_name() ||
      _paymentRequest->request_payer_email() ||
      _paymentRequest->request_payer_phone()) {
    // TODO(crbug.com/602666): User should get here only if they have selected
    // a contact info.
    DCHECK(_paymentRequest->selected_contact_profile());
    _pendingPaymentResponse.contactAddress =
        *_paymentRequest->selected_contact_profile();
    _addressNormalizationManager->StartNormalizingAddress(
        &_pendingPaymentResponse.contactAddress);
  }

  __weak PaymentRequestManager* weakSelf = self;
  _addressNormalizationManager->FinalizeWithCompletionCallback(
      base::BindBlockArc(^() {
        [weakSelf paymentRequestAddressNormalizationDidComplete];
      }));
}

- (void)paymentRequestAddressNormalizationDidComplete {
  web::PaymentResponse paymentResponse;

  // If the merchant specified the card network as part of the "basic-card"
  // payment method, return "basic-card" as the method_name. Otherwise, return
  // the name of the network directly.
  std::string issuer_network = autofill::data_util::GetPaymentRequestData(
                                   _pendingPaymentResponse.creditCard.network())
                                   .basic_card_issuer_network;
  paymentResponse.method_name =
      _paymentRequest->basic_card_specified_networks().find(issuer_network) !=
              _paymentRequest->basic_card_specified_networks().end()
          ? base::ASCIIToUTF16("basic-card")
          : base::ASCIIToUTF16(issuer_network);

  paymentResponse.details =
      payments::data_util::GetBasicCardResponseFromAutofillCreditCard(
          _pendingPaymentResponse.creditCard,
          _pendingPaymentResponse.verificationCode,
          _pendingPaymentResponse.billingAddress,
          GetApplicationContext()->GetApplicationLocale());

  if (_paymentRequest->request_shipping()) {
    paymentResponse.shipping_address =
        payments::data_util::GetPaymentAddressFromAutofillProfile(
            _pendingPaymentResponse.shippingAddress,
            GetApplicationContext()->GetApplicationLocale());

    web::PaymentShippingOption* shippingOption =
        _paymentRequest->selected_shipping_option();
    DCHECK(shippingOption);
    paymentResponse.shipping_option = shippingOption->id;
  }

  if (_paymentRequest->request_payer_name()) {
    paymentResponse.payer_name = _pendingPaymentResponse.contactAddress.GetInfo(
        autofill::AutofillType(autofill::NAME_FULL),
        GetApplicationContext()->GetApplicationLocale());
  }

  if (_paymentRequest->request_payer_email()) {
    paymentResponse.payer_email =
        _pendingPaymentResponse.contactAddress.GetRawInfo(
            autofill::EMAIL_ADDRESS);
  }

  if (_paymentRequest->request_payer_phone()) {
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
         didSelectShippingAddress:(payments::PaymentAddress)shippingAddress {
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
  [self enableCurrentWebState];
}

@end
