/*
 * Copyright 2016 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* global PaymentRequest:false */

/**
 * Launches the PaymentRequest UI that does not require a shipping address.
 */
function buy() {  // eslint-disable-line no-unused-vars
  try {
    new PaymentRequest(['visa'], {
      items: [{
        id: 'total',
        label: 'Total',
        amount: {currency: 'USD', value: '5.00'}
      }]
    })
        .show()
        .then(function(resp) {
          resp.complete(true)
              .then(function() {
                print(
                    resp.methodName + '<br>' +
                    JSON.stringify(resp.details, undefined, 2));
              })
              .catch(function(error) {
                print(error.message);
              });
        })
        .catch(function(error) {
          print(error.message);
        });
  } catch (error) {
    print(error.message);
  }
}
