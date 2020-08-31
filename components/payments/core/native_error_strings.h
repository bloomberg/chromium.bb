// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_NATIVE_ERROR_STRINGS_H_
#define COMPONENTS_PAYMENTS_CORE_NATIVE_ERROR_STRINGS_H_

namespace payments {
namespace errors {

// These strings are referenced only from C++.

// Mojo call PaymentRequest::Init() must precede PaymentRequest::Abort().
extern const char kCannotAbortWithoutInit[];

// Mojo call PaymentRequest::Show() must precede PaymentRequest::Abort().
extern const char kCannotAbortWithoutShow[];

// Mojo call PaymentRequest::Init() must precede
// PaymentRequest::CanMakePayment().
extern const char kCannotCallCanMakePaymentWithoutInit[];

// Mojo call PaymentRequest::Init() must precede
// PaymentRequest::HasEnrolledInstrument().
extern const char kCannotCallHasEnrolledInstrumentWithoutInit[];

// Mojo call PaymentRequest::Init() must precede PaymentRequest::Complete().
extern const char kCannotCompleteWithoutInit[];

// Mojo call PaymentRequest::Show() must precede PaymentRequest::Complete().
extern const char kCannotCompleteWithoutShow[];

// Used when "serviceworker"."scope" string A in web app manifest B is not a
// valid URL and cannot be resolved as a relative URL either. This format should
// be used with base::ReplaceStringPlaceholders(fmt, {A, B}, nullptr).
extern const char kCannotResolveServiceWorkerScope[];

// Used when "serviceworker"."src" string A in web app manifest B is not a valid
// URL and cannot be resolved as a relative URL either. This format should be
// used with base::ReplaceStringPlaceholders(fmt, {A, B}, nullptr).
extern const char kCannotResolveServiceWorkerUrl[];

// Mojo call PaymentRequest::Init() must precede PaymentRequest::Retry().
extern const char kCannotRetryWithoutInit[];

// Mojo call PaymentRequest::Show() must precede PaymentRequest::Retry().
extern const char kCannotRetryWithoutShow[];

// Used when a payment method A has a cross-origin "Link:
// rel=payment-method-manifest" to the manifest B. This format should be used
// with base::ReplaceStringPlaceholders(fmt, {B, A}, nullptr).
extern const char kCrossOriginPaymentMethodManifestNotAllowed[];

// The URL A in web app manifest B's "serviceworker"."scope" must be of the same
// origin as the web app manifest itself. This format should be used with
// base::ReplaceStringPlaceholders(fmt, {A, B}, nullptr).
extern const char kCrossOriginServiceWorkerScopeNotAllowed[];

// The URL A in web app manifest B's "serviceworker"."src" must be of the same
// origin as the web app manifest itself. This format should be used with
// base::ReplaceStringPlaceholders(fmt, {A, B}, nullptr).
extern const char kCrossOriginServiceWorkerUrlNotAllowed[];

// The "default_applications" list on origin A is not allowed to contain a URL
// from origin B. This format should be used with
// base::ReplaceStringPlaceholders(format, {B, A}, nullptr).
extern const char kCrossOriginWebAppManifestNotAllowed[];

// The format for a detailed message about invalid SSL certificate. This format
// should be used with base::ReplaceChars() function, where "$" is the character
// to replace.
extern const char kDetailedInvalidSslCertificateMessageFormat[];

// Used for HTTP redirects that are prohibited for payment method manifests.
// This format should be used with base::ReplaceStringPlaceholders(fmt,
// {http_code, http_code_phrase, original_url}, nullptr).
extern const char kHttpStatusCodeNotAllowed[];

// The "default_applications" list should contain exactly one URL for JIT
// install feature to work.
extern const char kInstallingMultipleDefaultAppsNotSupported[];

// PaymentRequest::Init() is called when the initiating RenderFrameHost no
// longer exists.
extern const char kInvalidInitiatorFrame[];

// Used to let the web developer know about an invalid payment manifest URL A.
// This format should be used with base::ReplaceStringPlaceholders(fmt, {A},
// nullptr).
extern const char kInvalidManifestUrl[];

// Web app manifest contains an empty or non-UTF8 service worker scope.
extern const char kInvalidServiceWorkerScope[];

// Web app manifest contains an empty or non-UTF8 service worker URL.
extern const char kInvalidServiceWorkerUrl[];

// Chrome refuses to provide any payment information to a website with an
// invalid SSL certificate.
extern const char kInvalidSslCertificate[];

// The downloaded web app icon should draw something for JIT install feature to
// work.
extern const char kInvalidWebAppIcon[];

// Used when the {"supportedMethods": "", data: {}} is required, but not
// provided.
extern const char kMethodDataRequired[];

// Used when non-empty "supportedMethods": "" is required, but not provided.
extern const char kMethodNameRequired[];

// The format for the message about multiple payment methods that are not
// supported. This format should be used with base::ReplaceChars() function,
// where "$" is the character to replace.
extern const char kMultiplePaymentMethodsNotSupportedFormat[];

// Payment handler did not respond to the "paymentrequest" event.
extern const char kNoResponseToPaymentEvent[];

// Used when PaymentRequest::Init() has not been called, but should have been.
extern const char kNotInitialized[];

// Used when PaymentRequest::Show() has not been called, but should have been.
extern const char kNotShown[];

// Used for errors about cross-site redirects from A to B. This format should be
// used with base::ReplaceStringPlaceholders(fmt, {A, B}, nullptr).
extern const char kPaymentManifestCrossSiteRedirectNotAllowed[];

// Used when downloading payment manifest URL A has failed. This format should
// be used with base::ReplaceStringPlaceholders(fmt, {A}, nullptr).
extern const char kPaymentManifestDownloadFailed[];

// Payment handler passed a non-object field "details" in response to the
// "paymentrequest" event.
extern const char kPaymentDetailsNotObject[];

// Payment handler passed a non-stringifiable field "details" in response to the
// "paymentrequest" event.
extern const char kPaymentDetailsStringifyError[];

// Used when the browser failed to fire the "paymentrequest" event without any
// actionable corrective action from the web developer.
extern const char kPaymentEventBrowserError[];

// Service worker timed out or stopped for some reason or was killed before the
// payment handler could respond to the "paymentrequest" event.
extern const char kPaymentEventServiceWorkerError[];

// Service worker timed out while responding to "paymentrequest" event.
extern const char kPaymentEventTimeout[];

// Payment handler navigated to a page with insecure context, invalid SSL, or
// malicious content.
extern const char kPaymentHandlerInsecureNavigation[];

// Payment handler encountered an internal error when handling the
// "paymentrequest" event.
extern const char kPaymentEventInternalError[];

// Payment handler rejected the promise passed into
// PaymentRequestEvent.respondWith() method.
extern const char kPaymentEventRejected[];

// Used when maximum number of redirects has been reached.
extern const char kReachedMaximumNumberOfRedirects[];

// The format for the message about a single payment method that is not
// supported. This format should be used with base::ReplaceChars() function,
// where "$" is the character to replace.
extern const char kSinglePaymentMethodNotSupportedFormat[];

// Used when non-empty "shippingOptionId": "" is required, but not provided.
extern const char kShippingOptionIdRequired[];

// The payment handler rejected the promise passed into
// CanMakePaymentEvent.respondWith().
extern const char kCanMakePaymentEventRejected[];

// The payment handler timed out responding to "canmakepayment" event.
extern const char kCanMakePaymentEventTimeout[];

// The payment handler did not respond to the "canmakepayment" event.
extern const char kCanMakePaymentEventNoResponse[];

// The payment handler did not specify a value for "readyForMinimalUI" field.
extern const char kCanMakePaymentEventNoReadyForMinimalUiValue[];

// The payment handler called CanMakePaymentEvent.respondWith(value) with a
// non-boolean value.
extern const char kCanMakePaymentEventBooleanConversionError[];

// Browser encountered an error when firing the "canmakepayment" event.
extern const char kCanMakePaymentEventBrowserError[];

// The payment handler threw a JavaScript exception while handling the
// "canmakepayment" event.
extern const char kCanMakePaymentEventInternalError[];

// The payment handler specified an invalid value for "accountBalance".
extern const char kCanMakePaymentEventInvalidAccountBalanceValue[];

// The payment handler called CanMakePaymentEvent.respondWithMinimalUI(value)
// with a value that could not be converted into a JavaScript dictionary with
// values for "canMakePayment", "readyForMinimalUI", and "accountBalance".
extern const char kCanMakePaymentEventMinimalUiResponseConversionError[];

// The payment handler did not specify a value for "accountBalance".
extern const char kCanMakePaymentEventNoAccountBalanceValue[];

// The payment handler did not specify a value for "canMakePayment" field in
// CanMakePaymentEvent.respondWithMinimalUI().
extern const char kCanMakePaymentEventNoCanMakePaymentValue[];

// Browser does not fire the "canmakepayment" event if the payment handler does
// not support any URL-based payment methods.
extern const char kCanMakePaymentEventNoUrlBasedPaymentMethods[];

// Browser does not fire the "canmakepayment" event for just-in-time installable
// payment handlers.
extern const char kCanMakePaymentEventNotInstalled[];

// Browser fires the "canmakepayment" event only for explicitly verified payment
// methods, i.e., not when "supportedOrigins": "*".
extern const char kCanMakePaymentEventNoExplicitlyVerifiedMethods[];

// A message about unsupported payment method.
extern const char kGenericPaymentMethodNotSupportedMessage[];

// Used for errors downloading the payment method manifest. This format should
// be used with base::ReplaceStringPlaceholders(fmt, {A}, nullptr).
extern const char kNoContentAndNoLinkHeader[];

// User when the downloaded payment manifest A is empty. This format should be
// used with base::ReplaceStringPlaceholders(fmt, {A}, nullptr).
extern const char kNoContentInPaymentManifest[];

}  // namespace errors
}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_NATIVE_ERROR_STRINGS_H_
