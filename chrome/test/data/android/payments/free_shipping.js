/*
 * Copyright 2016 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* global PaymentRequest:false */
/* global toDictionary:false */

/**
 * Launches the PaymentRequest UI that offers free shipping worldwide.
 */
function buy() {  // eslint-disable-line no-unused-vars
  try {
    var request = new PaymentRequest(
        [{supportedMethods: ['visa']}], {
          total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
          shippingOptions: [{
            id: 'freeShippingOption',
            label: 'Free global shipping',
            amount: {currency: 'USD', value: '0'},
            selected: true
          }]
        },
        {requestShipping: true});
    request.show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(
                    resp.shippingOption + '<br>' +
                    JSON.stringify(
                        toDictionary(resp.shippingAddress), undefined, 2) +
                    '<br>' + resp.methodName + '<br>' +
                    JSON.stringify(resp.details, undefined, 2));
              })
              .catch(function(error) {
                print(error);
              });
        })
        .catch(function(error) {
          print(error);
        });
  } catch (error) {
    print(error.message);
  }
}
