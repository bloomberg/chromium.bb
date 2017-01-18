// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview JavaScript implementation of the Payment Request API. Conforms
 * to the 18 July 2016 editor's draft at
 * https://w3c.github.io/browser-payment-api/.
 *
 * This is a minimal implementation that sends data to the app side to present
 * the user interface. When loaded, installs the API onto the window object.
 */

// Namespace for all PaymentRequest implementations. __gCrWeb must have already
// been defined.
__gCrWeb.paymentRequestManager = {
  /**
   * The pending PaymentRequest, if any. Used by the app side to invoke the
   * associated resolve or reject function.
   * @type {window.PaymentRequest}
   */
  pendingRequest: null,

  /**
   * The pending PaymentResponse, if any. Used by the app side to invoke the
   * associated resolve function.
   * @type {window.PaymentResponse}
   */
  pendingResponse: null
};
__gCrWeb['paymentRequestManager'] = __gCrWeb.paymentRequestManager;

/** @typedef {{
 *     methodData: !Array.<window.PaymentMethodData>,
 *     details: !window.PaymentDetails,
 *     options: (window.PaymentOptions|undefined)
 * }}
 */
__gCrWeb.paymentRequestManager.SerializedPaymentRequest;

/** @typedef {{
 *     methodName: string,
 *     details: Object
 * }}
 */
__gCrWeb.paymentRequestManager.SerializedPaymentResponse;

/**
 * A request to make a payment.
 *
 * @param {!Array.<window.PaymentMethodData>} methodData Payment method
 *     identifiers for the payment methods that the web site accepts and any
 *     associated payment method specific data.
 * @param {!window.PaymentDetails} details Information about the transaction
 *     that the user is being asked to complete such as the line items in an
 *     order.
 * @param {window.PaymentOptions=} opt_options Information about what
 *     options the web page wishes to use from the payment request system.
 * @constructor
 * @implements {EventTarget}
 */
window.PaymentRequest = function(methodData, details, opt_options) {
  if (window.top != window.self) {
    throw new Error(
        'PaymentRequest can only be used in the top-level context.');
  }

  if (methodData.length == 0)
    throw new Error('The length of the methodData parameter must be > 0.');

  for (var i = 0; i < methodData.length; i++) {
    if (methodData[i].supportedMethods.length == 0)
      throw new Error(
          'The length of the supportedMethods parameter must be > 0.');
  }

  // TODO(crbug.com/602666): Perform other validation per spec.

  /**
   * @type {!Array.<window.PaymentMethodData>}
   */
  this.methodData = methodData;

  /**
   * @type {!window.PaymentDetails}
   */
  this.details = details;

  /**
   * @type {?window.PaymentOptions}
   */
  this.options = opt_options || null;

  /**
   * Shipping address selected by the user.
   * @type {?window.PaymentAddress}
   */
  this.shippingAddress = null;

  /**
   * Shipping option selected by the user.
   * @type {?string}
   */
  this.shippingOption = null;

  /**
   * The state of this request, used to govern its lifecycle.
   * TODO(crbug.com/602666): Implement state transitions per spec.
   * @type {string}
   * @private
   */
  this.state = 'created';

  /**
   * The pending resolve function provided to the Promise returned by show().
   * @type {?function(window.PaymentResponse)}
   * @private
   */
  this.resolve_ = null;

  /**
   * The pending reject function provided to the Promise returned by show().
   * @type {?function(string)}
   * @private
   */
  this.reject_ = null;
};

// TODO(crbug.com/602666): Add the event handling logic.
window.PaymentRequest.prototype.addEventListener = function(type, listener) {
};

window.PaymentRequest.prototype.removeEventListener = function(type, listener) {
};

window.PaymentRequest.prototype.dispatchEvent = function(event) {
};

/**
 * Presents the PaymentRequest UI to the user.
 * @return {!Promise<window.PaymentResponse>} A promise to notify the caller of
 *     whether the user accepted or rejected the request.
 */
window.PaymentRequest.prototype.show = function() {
  if (__gCrWeb['paymentRequestManager'].pendingRequest) {
    throw new Error('Only one PaymentRequest may be shown at a time.');
  }
  __gCrWeb['paymentRequestManager'].pendingRequest = this;

  var message = {
    'command': 'paymentRequest.requestShow',
    'payment_request': this.serialize()
  };
  __gCrWeb.message.invokeOnHost(message);

  var self = this;
  return new Promise(function(resolve, reject) {
    self.resolve_ = resolve;
    self.reject_ = reject;
  });
};

/**
 * Resolves the pending Promise. Should only be called by the app-side logic.
 * @param {!__gCrWeb.paymentRequestManager.SerializedPaymentResponse}
 *     paymentResponseData The response to provide to the resolve function of
 *     the Promise.
 */
window.PaymentRequest.prototype.resolve = function(paymentResponseData) {
  if (!paymentResponseData) {
    throw new Error("Internal PaymentRequest error: PaymentResponse missing.");
  }

  if (!this.resolve_) {
    throw new Error("Internal PaymentRequest error: resolve function missing.");
  }

  var paymentResponse = null;
  try {
    paymentResponse = window.PaymentResponse.parse(paymentResponseData);
  } catch (e) {
    throw new Error(
        "Internal PaymentRequest error: failed to parse PaymentResponse data.");
  }
  this.resolve_(paymentResponse);

  this.resolve_ = null;
  this.reject_ = null;
  __gCrWeb['paymentRequestManager'].pendingRequest = null;
  __gCrWeb['paymentRequestManager'].pendingResponse = paymentResponse;
}

/**
 * Rejects the pending Promise. Should only be called by the app-side logic.
 * @param {!string} message An error message explaining why the Promise is being
 *     rejected.
 */
window.PaymentRequest.prototype.reject = function(message) {
  if (!this.reject_) {
    throw new Error("Internal PaymentRequest error: reject function missing.");
  }

  this.reject_(message);

  this.resolve_ = null;
  this.reject_ = null;
  __gCrWeb['paymentRequestManager'].pendingRequest = null;
}

/**
 * Returns a simple object representation of this payment request suitable for
 * sending to the app side.
 * @return {__gCrWeb.paymentRequestManager.SerializedPaymentRequest}
 */
window.PaymentRequest.prototype.serialize = function() {
  var serialized = {
    'methodData': this.methodData,
    'details': this.details,
  };
  if (this.options)
    serialized['options'] = this.options;
  return serialized;
};

/** @typedef {{
 *     supportedMethods: Array<string>,
 *     data: (Object|undefined)
 * }}
 */
window.PaymentMethodData;

/** @typedef {{
 *     currency: string,
 *     value: string
 * }}
 */
window.PaymentCurrencyAmount;

/** @typedef {{
 *     total: (window.PaymentItem|undefined),
 *     displayItems: (Array<window.PaymentItem>|undefined),
 *     shippingOptions: (Array<window.PaymentShippingOption>|undefined),
 *     modifiers: (Array<window.PaymentDetailsModifier>|undefined)
 * }}
 */
window.PaymentDetails;

/** @typedef {{
 *     supportedMethods: Array<string>,
 *     total: (window.PaymentItem|undefined),
 *     additionalDisplayItems: (Array<window.PaymentItem>|undefined)
 * }}
 */
window.PaymentDetailsModifier;

/** @typedef {{
 *     requestPayerEmail: (boolean|undefined),
 *     requestPayerPhone: (boolean|undefined),
 *     requestShipping: (boolean|undefined)
 * }}
 */
window.PaymentOptions;

/** @typedef {{
 *     label: string,
 *     amount: window.PaymentCurrencyAmount
 * }}
 */
window.PaymentItem;

// TODO(crbug.com/602666): Convert this to a class with readonly properties.
/** @typedef {{
 *     country: string,
 *     addressLine: Array<string>,
 *     region: string,
 *     city: string,
 *     dependentLocality: string,
 *     postalCode: string,
 *     sortingCode: string,
 *     languageCode: string,
 *     organization: string,
 *     recipient: string,
 *     phone: string
 * }}
 */
window.PaymentAddress;

/** @typedef {{
 *     id: string,
 *     label: string,
 *     amount: window.PaymentCurrencyAmount,
 *     selected: boolean
 * }}
 */
window.PaymentShippingOption;

/**
 * A response to a request to make a payment. Represents the choices made by the
 * user and provided to the web page through the resolve function of the Promise
 * returned by PaymentRequest.show().
 *
 * @param {string} methodName The payment method identifier for the payment
 *     method that the user selected to fulfil the transaction.
 * @param {Object} details An object that provides a payment method specific
 *     message used by the merchant to process the transaction and determine
 *     successful fund transfer.
 * @constructor
 * @private
 */
window.PaymentResponse = function(methodName, details) {
  /**
   * @type {string}
   */
  this.methodName = methodName;

  /**
   * @type {Object}
   */
  this.details = details;

  /**
   * The pending resolve function provided to the Promise returned by
   * complete().
   * @type {?function()}
   * @private
   */
  this.resolve_ = null;
};

/**
 * Communicates the result of processing the payment.
 * @param {boolean} success Indicates whether processing succeeded.
 * @return {!Promise} A promise to notify the caller when the user interface has
 *     been closed.
 */
window.PaymentResponse.prototype.complete = function(success) {
  var message = {
    'command': 'paymentRequest.responseComplete'
  };
  __gCrWeb.message.invokeOnHost(message);

  var self = this;
  return new Promise(function(resolve, reject) {
    self.resolve_ = resolve;
    // Any reject function provided is ignored because the spec includes no
    // provision for rejecting the response promise. User agents are directed to
    // always resolve the promise.
  });
};

/**
 * Resolves the pending Promise. Should only be called by the app-side logic.
 */
window.PaymentResponse.prototype.resolve = function() {
  // If the page has not yet provided a resolve function, do nothing. This can
  // happen in the case where the UI times out while waiting for the page to
  // call complete().
  if (!this.resolve_) {
    return;
  }

  this.resolve_();

  this.resolve_ = null;
  __gCrWeb['paymentRequestManager'].pendingResponse = null;
}

/**
 * Parses |paymentResponseData| into a PaymentResponse object.
 * @param {!__gCrWeb.paymentRequestManager.SerializedPaymentResponse}
 *     paymentResponseData A simple object representation of a PaymentResponse.
 * @return {window.PaymentResponse} A PaymentResponse object.
 */
window.PaymentResponse.parse = function(paymentResponseData) {
  return new window.PaymentResponse(paymentResponseData['methodName'],
                                    paymentResponseData['details']);
};
