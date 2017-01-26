/*
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* global PaymentRequest:false */

/**
 * Launches the PaymentRequest UI with Bob Pay as the only payment method.
 *
 * <p>When the developer chooses a single payment method and requests no other
 * data (no shipping, no email, no phone, ...), Payment Request will apply
 * the UI skip optimization, skipping its own UI enterily and going directly to
 * Bob Pay.
 */
function buy() {  // eslint-disable-line no-unused-vars
  try {
    new PaymentRequest(
        [{supportedMethods: ['https://bobpay.com']}],
        {total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}}})
        .show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(resp.methodName + '<br>' +
                      JSON.stringify(resp.details, undefined, 2));
              })
              .catch(function(error) {
                print('complete() rejected<br>' + error);
              });
        })
        .catch(function(error) {
          print('show() rejected<br>' + error);
        });
  } catch (error) {
    print('exception thrown<br>' + error);
  }
}
