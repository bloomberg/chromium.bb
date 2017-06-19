// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview JavaScript implementation of the Payment Request API. When
 * loaded, installs the API onto the window object. Conforms
 * to https://www.w3.org/TR/payment-request/. Note: This is a work in progress.
 */

/**
 * This class implements the DOM level 2 EventTarget interface. The
 * Implementation is copied form src/ui/webui/resources/js/cr/event_target.js.
 * This code should be removed once there is a plan to move event_target.js out
 * of WebUI and reuse in iOS.
 * @constructor
 * @implements {EventTarget}
 */
__gCrWeb.EventTarget =
    function() {
  this.listeners_ = null;
}

// Store EventTarget namespace object in a global __gCrWeb object referenced
// by a string, so it does not get renamed by closure compiler during the
// minification.
__gCrWeb['EventTarget'] = __gCrWeb.EventTarget;

/* Beginning of anonymous object. */
(function() {
  __gCrWeb.EventTarget.prototype = {
    /** @override */
    addEventListener: function(type, handler) {
      if (!this.listeners_)
        this.listeners_ = Object.create(null);
      if (!(type in this.listeners_)) {
        this.listeners_[type] = [handler];
      } else {
        var handlers = this.listeners_[type];
        if (handlers.indexOf(handler) < 0)
          handlers.push(handler);
      }
    },

    /** @override */
    removeEventListener: function(type, handler) {
      if (!this.listeners_)
        return;
      if (type in this.listeners_) {
        var handlers = this.listeners_[type];
        var index = handlers.indexOf(handler);
        if (index >= 0) {
          // Clean up if this was the last listener.
          if (handlers.length == 1)
            delete this.listeners_[type];
          else
            handlers.splice(index, 1);
        }
      }
    },

    /** @override */
    dispatchEvent: function(event) {
      var type = event.type;
      var prevented = 0;

      if (!this.listeners_)
        return true;

      // Override the DOM Event's 'target' property.
      Object.defineProperty(event, 'target', {value: this});

      if (type in this.listeners_) {
        // Clone to prevent removal during dispatch
        var handlers = this.listeners_[type].concat();
        for (var i = 0, handler; handler = handlers[i]; i++) {
          if (handler.handleEvent)
            prevented |= handler.handleEvent.call(handler, event) === false;
          else
            prevented |= handler.call(this, event) === false;
        }
      }

      return !prevented && !event.defaultPrevented;
    }
  };
}());  // End of anonymous object

/**
 * PromiseResolver is a helper class that allows creating a Promise that will be
 * fulfilled (resolved or rejected) some time later. The implementation is
 * copied from src/ui/webui/resources/js/promise_resolver.js. This code should
 * be removed once there is a plan to move promise_resolver.js out of WebUI and
 * reuse in iOS.
 * @constructor
 * @template T
 */
__gCrWeb.PromiseResolver =
    function() {
  /** @private {function(T=): void} */
  this.resolve_;

  /** @private {function(*=): void} */
  this.reject_;

  /** @private {!Promise<T>} */
  this.promise_ = new Promise(function(resolve, reject) {
    this.resolve_ = resolve;
    this.reject_ = reject;
  }.bind(this));
}

// Store PromiseResolver namespace object in a global __gCrWeb object referenced
// by a string, so it does not get renamed by closure compiler during the
// minification.
__gCrWeb['PromiseResolver'] = __gCrWeb.PromiseResolver;

/* Beginning of anonymous object. */
(function() {
  __gCrWeb.PromiseResolver.prototype = {
    /** @return {!Promise<T>} */
    get promise() { return this.promise_; },
    /** @return {function(T=): void} */
    get resolve() { return this.resolve_; },
    /** @return {function(*=): void} */
    get reject() { return this.reject_; },
  };
}());  // End of anonymous object

/**
 * A simple object representation of |window.PaymentRequest| meant for
 * communication to the app side.
 * @typedef {{
 *   methodData: !Array<!window.PaymentMethodData>,
 *   details: !window.PaymentDetails,
 *   options: (window.PaymentOptions|undefined)
 * }}
 */
var SerializedPaymentRequest;

/**
 * A simple object representation of |window.PaymentResponse| meant for
 * communication to the app side.
 * @typedef {{
 *   paymentRequestId: string,
 *   methodName: string,
 *   details: Object,
 *   shippingAddress: (!window.PaymentAddress|undefined),
 *   shippingOption: (string|undefined),
 *   payerName: (string|undefined),
 *   payerEmail: (string|undefined),
 *   payerPhone: (string|undefined)
 * }}
 */
var SerializedPaymentResponse;

/* Beginning of anonymous object. */
(function() {
  // Namespace for all PaymentRequest implementations. __gCrWeb must have
  // already
  // been defined.
  __gCrWeb.paymentRequestManager = {};

  // Store paymentRequestManager namespace object in a global __gCrWeb object
  // referenced by a string, so it does not get renamed by closure compiler
  // during
  // the minification.
  __gCrWeb['paymentRequestManager'] = __gCrWeb.paymentRequestManager;

  /**
   * The PaymentRequest object, if any. This object is provided by the page and
   * only updated by the app side.
   * @type {window.PaymentRequest}
   */
  __gCrWeb['paymentRequestManager'].pendingRequest = null;

  /**
   * The PromiseResolver object used to resolve or reject the promise returned
   * by PaymentRequest.prototype.show, if any.
   * @type {__gCrWeb.PromiseResolver}
   */
  __gCrWeb['paymentRequestManager'].requestPromiseResolver = null;

  /**
   * The PromiseResolver object used to resolve or reject the promise returned
   * by PaymentRequest.prototype.abort, if any.
   * @type {__gCrWeb.PromiseResolver}
   */
  __gCrWeb['paymentRequestManager'].abortPromiseResolver = null;

  /**
   * The PromiseResolver object used to resolve the promise returned by
   * PaymentResponse.prototype.complete, if any.
   * @type {window.PaymentRequest}
   */
  __gCrWeb['paymentRequestManager'].responsePromiseResolver = null;

  /**
   * Parses |paymentResponseData| into a window.PaymentResponse object.
   * @param {!SerializedPaymentResponse} paymentResponseData
   * @return {window.PaymentResponse}
   */
  __gCrWeb['paymentRequestManager'].parsePaymentResponseData = function(
      paymentResponseData) {
    var response = new window.PaymentResponse(
        paymentResponseData['paymentRequestID'],
        paymentResponseData['methodName'], paymentResponseData['details']);
    if (paymentResponseData['shippingAddress'])
      response.shippingAddress = paymentResponseData['shippingAddress'];
    if (paymentResponseData['shippingOption'])
      response.shippingOption = paymentResponseData['shippingOption'];
    if (paymentResponseData['payerName'])
      response.payerName = paymentResponseData['payerName'];
    if (paymentResponseData['payerEmail'])
      response.payerEmail = paymentResponseData['payerEmail'];
    if (paymentResponseData['payerPhone'])
      response.payerPhone = paymentResponseData['payerPhone'];

    return response;
  };

  /**
   * The event that enables the web page to update the details of the payment
   * request in response to a user interaction.
   * @type {Event}
   */
  __gCrWeb['paymentRequestManager'].updateEvent = null;

  /**
   * Handles invocation of updateWith() on the updateEvent object. Updates the
   * payment details. Throws an error if |detailsOrPromise| is not a valid
   * instance of window.PaymentDetails or it is a promise that does not fulfill
   * with a valid one.
   * @param {!Promise<!window.PaymentDetails>|!window.PaymentDetails}
   *     detailsOrPromise
   */
  __gCrWeb['paymentRequestManager'].updateWith = function(detailsOrPromise) {
    // if |detailsOrPromise| is not an instance of a Promise, wrap it in a
    // Promise that fulfills with |detailsOrPromise|.
    if (!detailsOrPromise || !(detailsOrPromise.then instanceof Function) ||
        !(detailsOrPromise.catch instanceof Function)) {
      detailsOrPromise = Promise.resolve(detailsOrPromise);
    }

    detailsOrPromise
        .then(function(paymentDetails) {
          if (!paymentDetails)
            throw new TypeError(
                'An instance of PaymentDetails must be provided.');

          var message = {
            'command': 'paymentRequest.updatePaymentDetails',
            'payment_details': paymentDetails,
          };
          __gCrWeb.message.invokeOnHost(message);

          __gCrWeb['paymentRequestManager'].updateEvent = null;
        })
        .catch(function() {
          var message = {
            'command': 'paymentRequest.requestAbort',
          };
          __gCrWeb.message.invokeOnHost(message);

          __gCrWeb['paymentRequestManager'].updateEvent = null;
        });
  };

  /**
   * Resolves the pending PaymentRequest.
   * @param {!SerializedPaymentResponse} paymentResponseData The response to
   *     provide to the resolve function of the Promise.
   */
  __gCrWeb['paymentRequestManager'].resolveRequestPromise = function(
      paymentResponseData) {
    if (!__gCrWeb['paymentRequestManager'].requestPromiseResolver) {
      throw new Error('Internal PaymentRequest error: No Promise to resolve.');
    }

    if (!paymentResponseData) {
      __gCrWeb['paymentRequestManager'].rejectRequestPromise(
          'Internal PaymentRequest error: PaymentResponse missing.');
    }

    var paymentResponse = null;
    try {
      paymentResponse =
          __gCrWeb['paymentRequestManager'].parsePaymentResponseData(
              paymentResponseData);
    } catch (e) {
      __gCrWeb['paymentRequestManager'].rejectRequestPromise(
          'Internal PaymentRequest error: failed to parse PaymentResponse.');
    }

    __gCrWeb['paymentRequestManager'].responsePromiseResolver =
        new __gCrWeb.PromiseResolver();
    __gCrWeb['paymentRequestManager'].requestPromiseResolver.resolve(
        paymentResponse);
    __gCrWeb['paymentRequestManager'].requestPromiseResolver = null;
    __gCrWeb['paymentRequestManager'].pendingRequest = null;
    __gCrWeb['paymentRequestManager'].updateEvent = null;
  };

  /**
   * Rejects the pending PaymentRequest.
   * @param {string} message An error message explaining why the Promise is
   * being rejected.
   */
  __gCrWeb['paymentRequestManager'].rejectRequestPromise = function(message) {
    if (!__gCrWeb['paymentRequestManager'].requestPromiseResolver) {
      throw new Error(
          'Internal PaymentRequest error: No Promise to reject. ',
          'Message was: ', message);
    }

    __gCrWeb['paymentRequestManager'].requestPromiseResolver.reject(message);
    __gCrWeb['paymentRequestManager'].requestPromiseResolver = null;
    __gCrWeb['paymentRequestManager'].pendingRequest = null;
    __gCrWeb['paymentRequestManager'].updateEvent = null;
  };

  /**
   * Resolves the promise returned by calling PaymentRequest.prototype.abort.
   * This method also gets called when the request is aborted through rejecting
   * the promise passed to PaymentRequestUpdateEvent.prototype.updateWith.
   * Therefore, it does nothing if there is no abort promise to resolve.
   */
  __gCrWeb['paymentRequestManager'].resolveAbortPromise = function() {
    if (!__gCrWeb['paymentRequestManager'].abortPromiseResolver)
      return;

    __gCrWeb['paymentRequestManager'].abortPromiseResolver.resolve();
    __gCrWeb['paymentRequestManager'].abortPromiseResolver = null;
  };

  /**
   * Resolves the promise returned by calling canMakePayment on the current
   * PaymentRequest.
   * @param {boolean} value The response to provide to the resolve function of
   *     the Promise.
   */
  __gCrWeb['paymentRequestManager'].resolveCanMakePaymentPromise = function(
      value) {
    if (!__gCrWeb['paymentRequestManager'].canMakePaymentPromiseResolver) {
      throw new Error('Internal PaymentRequest error: No Promise to resolve.');
    }

    __gCrWeb['paymentRequestManager'].canMakePaymentPromiseResolver.resolve(
        value);
    __gCrWeb['paymentRequestManager'].canMakePaymentPromiseResolver = null;
  };

  /**
   * Rejects the promise returned by calling canMakePayment on the current
   * PaymentRequest.
   * @param {string} message An error message explaining why the Promise is
   *     being rejected.
   */
  __gCrWeb['paymentRequestManager'].rejectCanMakePaymentPromise = function(
      message) {
    if (!__gCrWeb['paymentRequestManager'].canMakePaymentPromiseResolver) {
      throw new Error(
          'Internal PaymentRequest error: No Promise to reject. ',
          'Message was: ', message);
    }

    __gCrWeb['paymentRequestManager'].canMakePaymentPromiseResolver.reject(
        message);
    __gCrWeb['paymentRequestManager'].canMakePaymentPromiseResolver = null;
  };

  /**
   * Serializes |paymentRequest| to a SerializedPaymentRequest object.
   * @param {window.PaymentRequest} paymentRequest
   * @return {SerializedPaymentRequest}
   */
  __gCrWeb['paymentRequestManager'].serializePaymentRequest = function(
      paymentRequest) {
    var serialized = {
      'methodData': paymentRequest.methodData,
      'details': paymentRequest.details,
    };
    if (paymentRequest.options)
      serialized['options'] = paymentRequest.options;
    return serialized;
  };

  /**
   * Resolves the pending PaymentResponse.
   */
  __gCrWeb['paymentRequestManager'].resolveResponsePromise = function() {
    if (!__gCrWeb['paymentRequestManager'].responsePromiseResolver) {
      throw new Error('Internal PaymentRequest error: No Promise to resolve.');
    }

    __gCrWeb['paymentRequestManager'].responsePromiseResolver.resolve();
    __gCrWeb['paymentRequestManager'].responsePromiseResolver = null;
  };

  /**
   * Updates the shipping_option property of the PaymentRequest object to
   * |shippingOptionID| and dispatches a shippingoptionchange event.
   * @param {string} shippingOptionID
   */
  __gCrWeb['paymentRequestManager'].updateShippingOptionAndDispatchEvent =
      function(shippingOptionID) {
    if (!__gCrWeb['paymentRequestManager'].pendingRequest) {
      __gCrWeb['paymentRequestManager'].rejectRequestPromise(
          'Internal PaymentRequest error: No pending request.');
    }

    var pendingRequest = __gCrWeb['paymentRequestManager'].pendingRequest;

    if (__gCrWeb['paymentRequestManager'].updateEvent) {
      __gCrWeb['paymentRequestManager'].rejectRequestPromise(
          'Internal PaymentRequest error: Only one update may take ' +
          'place at a time.');
    }

    pendingRequest.shippingOption = shippingOptionID;

    __gCrWeb['paymentRequestManager'].updateEvent = new Event(
        'shippingoptionchange', {'bubbles': true, 'cancelable': false});

    Object.defineProperty(__gCrWeb['paymentRequestManager'].updateEvent,
        'updateWith', {value: __gCrWeb['paymentRequestManager'].updateWith});

    // setTimeout() is used in order to return immediately. Otherwise the
    // dispatchEvent call waits for all event handlers to return, which could
    // cause a ReentryGuard failure.
    window.setTimeout(function() {
      pendingRequest.dispatchEvent(
          __gCrWeb['paymentRequestManager'].updateEvent);
    }, 0);
  };

  /**
   * Updates the shipping_address property of the PaymentRequest object to
   * |shippingAddress| and dispatches a shippingaddresschange event.
   * @param {!window.PaymentAddress} shippingAddress
   */
  __gCrWeb['paymentRequestManager'].updateShippingAddressAndDispatchEvent =
      function(shippingAddress) {
    if (!__gCrWeb['paymentRequestManager'].pendingRequest) {
      __gCrWeb['paymentRequestManager'].rejectRequestPromise(
          'Internal PaymentRequest error: No pending request.');
    }

    var pendingRequest = __gCrWeb['paymentRequestManager'].pendingRequest;

    if (__gCrWeb['paymentRequestManager'].updateEvent) {
      __gCrWeb['paymentRequestManager'].rejectRequestPromise(
          'Internal PaymentRequest error: Only one update may take ' +
          'place at a time.');
    }

    pendingRequest.shippingAddress = shippingAddress;

    __gCrWeb['paymentRequestManager'].updateEvent = new Event(
        'shippingaddresschange', {'bubbles': true, 'cancelable': false});

    Object.defineProperty(__gCrWeb['paymentRequestManager'].updateEvent,
        'updateWith', {value: __gCrWeb['paymentRequestManager'].updateWith});

    // setTimeout() is used in order to return immediately. Otherwise the
    // dispatchEvent call waits for all event handlers to return, which could
    // cause a ReentryGuard failure.
    window.setTimeout(function() {
      pendingRequest.dispatchEvent(
          __gCrWeb['paymentRequestManager'].updateEvent);
    }, 0);
  };
}());  // End of anonymous object

/**
 * A request to make a payment.
 * @param {!Array<!window.PaymentMethodData>} methodData Payment method
 *     identifiers for the payment methods that the web site accepts and any
 *     associated payment method specific data.
 * @param {!window.PaymentDetails} details Information about the transaction
 *     that the user is being asked to complete such as the line items in an
 *     order.
 * @param {window.PaymentOptions=} opt_options Information about what
 *     options the web page wishes to use from the payment request system.
 * @constructor
 * @implements {EventTarget}
 * @extends {__gCrWeb.EventTarget}
 */
window.PaymentRequest = function(methodData, details, opt_options) {
  __gCrWeb.EventTarget.call(this);

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

  // TODO(crbug.com/602666): Process payment methods then set parsedMethodData
  // instead of methodData on this instance.
  // /**
  //  * @type {!Object<!Array<string>, ?string>}
  //  * @private
  //  */
  // this.parsedMethodData = ...;

  /**
   * @type {!Array<!window.PaymentMethodData>}
   * @private
   */
  this.methodData = methodData;

  /**
   * @type {!window.PaymentDetails}
   * @private
   */
  this.details = details;

  /**
   * @type {?window.PaymentOptions}
   * @private
   */
  this.options = opt_options || null;

  /**
   * The state of this request, used to govern its lifecycle.
   * TODO(crbug.com/602666): Implement state transitions per spec.
   * @type {string}
   * @private
   */
  this.state = 'created';

  /**
   * True if there is a pending updateWith call to update the payment request
   * and false otherwise.
   * TODO(crbug.com/602666): Implement changes in the value of this property per
   * spec.
   * @type {boolean}
   * @private
   */
  this.updating = false;

  /**
   * A provided or generated ID for the this Payment Request instance.
   * TODO(crbug.com/602666): Generate an ID if one is not provided.
   * @type {?string}
   */
  this.paymentRequestID = null;

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
   * Set to the value of shippingType property of |opt_options| if it is a valid
   * PaymentShippingType. Otherwise set to PaymentShippingType.SHIPPING.
   * @type {?PaymentShippingType}
   */
  this.shippingType = null;

  if (opt_options && opt_options.requestShipping) {
    if (opt_options.shippingType != PaymentShippingType.SHIPPING &&
        opt_options.shippingType != PaymentShippingType.DELIVERY &&
        opt_options.shippingType != PaymentShippingType.PICKUP) {
      this.shippingType = PaymentShippingType.SHIPPING;
    }
  }
};

window.PaymentRequest.prototype = {
  __proto__: __gCrWeb.EventTarget.prototype
};

/**
 * Presents the PaymentRequest UI to the user.
 * @return {!Promise<window.PaymentResponse>} A promise to notify the caller
 *     whether the user accepted or rejected the request.
 */
window.PaymentRequest.prototype.show = function() {
  if (__gCrWeb['paymentRequestManager'].pendingRequest) {
    throw new Error('Only one PaymentRequest may be shown at a time.');
  }
  __gCrWeb['paymentRequestManager'].pendingRequest = this;

  var message = {
    'command': 'paymentRequest.requestShow',
    'payment_request':
        __gCrWeb['paymentRequestManager'].serializePaymentRequest(this),
  };
  __gCrWeb.message.invokeOnHost(message);

  __gCrWeb['paymentRequestManager'].requestPromiseResolver =
      new __gCrWeb.PromiseResolver();
  return __gCrWeb['paymentRequestManager'].requestPromiseResolver.promise;
};

/**
 * May be called if the web page wishes to tell the user agent to abort the
 * payment request and to tear down any user interface that might be shown.
 * @return {!Promise<undefined>} A promise to notify the caller whether the user
 *     agent was able to abort the payment request.
 */
window.PaymentRequest.prototype.abort = function() {
  if (!__gCrWeb['paymentRequestManager'].pendingRequest) {
    __gCrWeb['paymentRequestManager'].rejectRequestPromise(
        'Internal PaymentRequest error: No pending request.');
  }

  var message = {
    'command': 'paymentRequest.requestAbort',
  };
  __gCrWeb.message.invokeOnHost(message);

  __gCrWeb['paymentRequestManager'].abortPromiseResolver =
      new __gCrWeb.PromiseResolver();
  return __gCrWeb['paymentRequestManager'].abortPromiseResolver.promise;
};

/**
 * May be called before calling show() to determine if the PaymentRequest object
 * can be used to make a payment.
 * @return {!Promise<boolean>} A promise to notify the caller whether the
 *     PaymentRequest object can be used to make a payment.
 */
window.PaymentRequest.prototype.canMakePayment = function() {
  // TODO(crbug.com/602666): return a promise rejected with InvalidStateError if
  // |this.state| != 'created'.

  var message = {
    'command': 'paymentRequest.requestCanMakePayment',
    'payment_request':
        __gCrWeb['paymentRequestManager'].serializePaymentRequest(this),
  };
  __gCrWeb.message.invokeOnHost(message);

  __gCrWeb['paymentRequestManager'].canMakePaymentPromiseResolver =
      new __gCrWeb.PromiseResolver();
  return __gCrWeb['paymentRequestManager'].canMakePaymentPromiseResolver
      .promise;
};

/**
 * @typedef {{
 *   supportedMethods: !Array<string>,
 *   data: (Object|undefined)
 * }}
 */
window.PaymentMethodData;

/**
 * @typedef {{
 *   currency: string,
 *   value: string,
 *   currencySystem: (string|undefined)
 * }}
 */
window.PaymentCurrencyAmount;

/**
 * @typedef {{
 *   total: (window.PaymentItem|undefined),
 *   displayItems: (!Array<!window.PaymentItem>|undefined),
 *   shippingOptions: (!Array<!window.PaymentShippingOption>|undefined),
 *   modifiers: (!Array<!window.PaymentDetailsModifier>|undefined),
 *   error: (string|undefined)
 * }}
 */
window.PaymentDetails;

/**
 * @typedef {{
 *   supportedMethods: !Array<string>,
 *   total: (window.PaymentItem|undefined),
 *   additionalDisplayItems: (!Array<!window.PaymentItem>|undefined)
 * }}
 */
window.PaymentDetailsModifier;

/**
 * Contains the possible values for affecting the payment request user interface
 * for gathering the shipping address if window.PaymentOptions.requestShipping
 * is set to true.
 * @enum {string}
 */
var PaymentShippingType = {
  SHIPPING: 'shipping',
  DELIVERY: 'delivery',
  PICKUP: 'pickup'
};

/**
 * @typedef {{
 *   requestPayerName: (boolean|undefined),
 *   requestPayerEmail: (boolean|undefined),
 *   requestPayerPhone: (boolean|undefined),
 *   requestShipping: (boolean|undefined),
 *   shippingType: (!PaymentShippingType|undefined)
 * }}
 */
window.PaymentOptions;

/**
 * @typedef {{
 *   label: string,
 *   amount: window.PaymentCurrencyAmount
 * }}
 */
window.PaymentItem;

/**
 * @typedef {{
 *   country: string,
 *   addressLine: !Array<string>,
 *   region: string,
 *   city: string,
 *   dependentLocality: string,
 *   postalCode: string,
 *   sortingCode: string,
 *   languageCode: string,
 *   organization: string,
 *   recipient: string,
 *   phone: string
 * }}
 */
window.PaymentAddress;

/**
 * @typedef {{
 *   id: string,
 *   label: string,
 *   amount: window.PaymentCurrencyAmount,
 *   selected: boolean
 * }}
 */
window.PaymentShippingOption;

/**
 * A response to a request to make a payment. Represents the choices made by the
 * user and provided to the web page through the resolve function of the Promise
 * returned by PaymentRequest.show().
 * https://w3c.github.io/browser-payment-api/#paymentresponse-interface
 * @param {string} methodName
 * @param {Object} details
 * @constructor
 * @private
 */
window.PaymentResponse = function(paymentRequestID, methodName, details) {
  /**
   * The same paymentRequestID present in the original window.PaymentRequest.
   * @type {string}
   */
  this.paymentRequestID = paymentRequestID;

  /**
   * The payment method identifier for the payment method that the user selected
   * to fulfil the transaction.
   * @type {string}
   */
  this.methodName = methodName;

  /**
   * An object that provides a payment method specific message used by the
   * merchant to process the transaction and determine successful fund transfer.
   * the payment request.
   * @type {Object}
   */
  this.details = details;

  /**
   * If the requestShipping flag was set to true in the window.PaymentOptions
   * passed to the window.PaymentRequest constructor, this will be the full and
   * final shipping address chosen by the user.
   * @type {?window.PaymentAddress}
   */
   this.shippingAddress = null;

  /**
   * If the requestShipping flag was set to true in the window.PaymentOptions
   * passed to the window.PaymentRequest constructor, this will be the id
   * attribute of the selected shipping option.
   * @type {?string}
   */
   this.shippingOption = null;

  /**
   * If the requestPayerName flag was set to true in the window.PaymentOptions
   * passed to the window.PaymentRequest constructor, this will be the name
   * provided by the user.
   * @type {?string}
   */
   this.payerName = null;

  /**
   * If the requestPayerEmail flag was set to true in the window.PaymentOptions
   * passed to the window.PaymentRequest constructor, this will be the email
   * address chosen by the user.
   * @type {?string}
   */
   this.payerEmail = null;

  /**
   * If the requestPayerPhone flag was set to true in the window.PaymentOptions
   * passed to the window.PaymentRequest constructor, this will be the phone
   * number chosen by the user.
   * @type {?string}
   */
   this.payerPhone = null;
};

/**
 * Contains the possible values for the string argument accepted by
 * window.PaymentResponse.prototype.complete.
 * @enum {string}
 */
var PaymentComplete = {
  SUCCESS: 'success',
  FAIL: 'fail',
  UNKNOWN: 'unknown'
};

/**
 * Communicates the result of processing the payment.
 * @param {PaymentComplete=} opt_result Indicates whether payment was
 *     successfully processed.
 * @return {!Promise} A promise to notify the caller when the user interface has
 *     been closed.
 */
window.PaymentResponse.prototype.complete = function(opt_result) {
  if (opt_result != PaymentComplete.UNKNOWN &&
      opt_result != PaymentComplete.SUCCESS &&
      opt_result != PaymentComplete.FAIL) {
    opt_result = PaymentComplete.UNKNOWN;
  }

  if (!__gCrWeb['paymentRequestManager'].responsePromiseResolver) {
    throw new Error('Internal PaymentRequest error: No Promise to return.');
  }

  var message = {
    'command': 'paymentRequest.responseComplete',
    'result': opt_result,
  };
  __gCrWeb.message.invokeOnHost(message);

  return __gCrWeb['paymentRequestManager'].responsePromiseResolver.promise;
};
