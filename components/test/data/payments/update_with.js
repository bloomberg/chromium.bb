/*
 * Copyright 2019 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Builds a PaymentRequest that requests a shipping address.
 * @return {PaymentRequest} - A new PaymentRequest object.
 */
function buildPaymentRequest() {
  try {
    return new PaymentRequest(
        [{supportedMethods: 'basic-card'}], {
          total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
          shippingOptions: [{
            selected: true,
            id: 'freeShipping',
            label: 'Free shipping',
            amount: {currency: 'USD', value: '0.00'},
          }],
        },
        {requestShipping: true});
  } catch (error) {
    print(error.message);
  }
}

/**
 * Shows the PaymentRequest.
 * @param {PaymentRequest} pr - The PaymentRequest object to show.
 */
function showPaymentRequest(pr) {
  pr.show()
      .then(function(resp) {
        resp.complete('success')
            .then(function() {
              print(JSON.stringify(resp, undefined, 2));
            })
            .catch(function(error) {
              print(error);
            });
      })
      .catch(function(error) {
        print(error);
      });
}

/**
 * Calls updateWith() with {}
 */
function updateWithEmpty() {  // eslint-disable-line no-unused-vars
  var pr = buildPaymentRequest();
  var updatedDetails = {};
  pr.addEventListener('shippingaddresschange', function(e) {
    e.updateWith(updatedDetails);
  });
  pr.addEventListener('shippingoptionchange', function(e) {
    e.updateWith(updatedDetails);
  });
  showPaymentRequest(pr);
}

/**
 * Calls updateWith() with total
 */
function updateWithTotal() {  // eslint-disable-line no-unused-vars
  var pr = buildPaymentRequest();
  var updatedDetails = {
    total: {label: 'Updated total', amount: {currency: 'USD', value: '10.00'}},
  };
  pr.addEventListener('shippingaddresschange', function(e) {
    e.updateWith(updatedDetails);
  });
  pr.addEventListener('shippingoptionchange', function(e) {
    e.updateWith(updatedDetails);
  });
  showPaymentRequest(pr);
}

/**
 * Calls updateWith() with shipping options
 */
function updateWithShippingOptions() {  // eslint-disable-line no-unused-vars
  var pr = buildPaymentRequest();
  var updatedDetails = {
    shippingOptions: [{
      selected: true,
      id: 'updatedShipping',
      label: 'Updated shipping',
      amount: {currency: 'USD', value: '5.00'},
    }],
  };
  pr.addEventListener('shippingaddresschange', function(e) {
    e.updateWith(updatedDetails);
  });
  pr.addEventListener('shippingoptionchange', function(e) {
    e.updateWith(updatedDetails);
  });
  showPaymentRequest(pr);
}
