/*
 * Copyright 2016 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* global PaymentRequest:false */

var request;

/**
 * Launches the PaymentRequest UI.
 */
function buy() {  // eslint-disable-line no-unused-vars
  try {
    request = new PaymentRequest(
        [{supportedMethods: ['visa']}],
        {total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}}});
    request.show();
  } catch (error) {
    print(error.message);
  }
}

/**
 * Aborts the PaymentRequest UI.
 */
function abort() {  // eslint-disable-line no-unused-vars
  try {
    request.abort().then(() => {
      print('Aborted');
    }).catch(() => {
      print('Cannot abort');
    });
  } catch (error) {
    print(error.message);
  }
}
