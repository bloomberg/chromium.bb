/*
 * Copyright 2019 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

const methodName = window.location.origin + '/pay';
const swSrcUrl = 'payment_handler_sw.js';

/** Installs the payment handler. */
async function install() { // eslint-disable-line no-unused-vars
  try {
    let registration =
        await navigator.serviceWorker.getRegistration(swSrcUrl);
    if (registration) {
      return 'The payment handler is already installed.';
    }

    await navigator.serviceWorker.register(swSrcUrl);
    registration = await navigator.serviceWorker.ready;
    if (!registration.paymentManager) {
      return 'PaymentManager API not found.';
    }

    await registration.paymentManager.instruments.set('instrument-id', {
      name: 'Instrument Name',
      method: methodName,
    });
    return 'success';
  } catch (e) {
    return e.toString();
  }
}

/**
 * Delegates handling of the provided options to the payment handler.
 * @param {Array<string>} delegations The list of payment options to delegate.
 * @return {string} The 'success' or error message.
 */
async function enableDelegations(delegations) { // eslint-disable-line no-unused-vars, max-len
  try {
    await navigator.serviceWorker.ready;
    let registration =
        await navigator.serviceWorker.getRegistration(swSrcUrl);
    if (!registration) {
      return 'The payment handler is not installed.';
    }
    if (!registration.paymentManager) {
      return 'PaymentManager API not found.';
    }
    if (!registration.paymentManager.enableDelegations) {
      return 'PaymentManager does not support enableDelegations method';
    }

    await registration.paymentManager.enableDelegations(delegations);
    return 'success';
  } catch (e) {
    return e.toString();
  }
}

/**
 * Launches the payment handler.
 */
async function launch() { // eslint-disable-line no-unused-vars
  try {
    const request = new PaymentRequest([{supportedMethods: methodName}], {
      total: {label: 'Total', amount: {currency: 'USD', value: '0.01'}},
    });
    const response = await request.show();
    await response.complete('success');
    return 'success';
  } catch (e) {
    return e.toString();
  }
}
