/*
 * Copyright 2016 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Merchant checks for ability to pay using debit cards.
 */
function checkBasicDebit() { // eslint-disable-line no-unused-vars
  try {
    new PaymentRequest(
        [{
          supportedMethods: ['basic-card'],
          data: {
            supportedTypes: ['debit'],
          },
        }], {
          total: {
            label: 'Total',
            amount: {
              currency: 'USD',
              value: '5.00',
            },
          },
        })
      .canMakePayment()
      .then(function(result) {
        print(result);
      })
      .catch(function(error) {
        print(error);
      });
  } catch (error) {
    print(error);
  }
}

/**
 * Merchant checks for ability to pay using "basic-card" with "mastercard" as
 * the supported network.
 */
function checkBasicMasterCard() { // eslint-disable-line no-unused-vars
  try {
    new PaymentRequest(
        [{
          supportedMethods: ['basic-card'],
          data: {
            supportedNetworks: ['mastercard'],
          },
        }], {
          total: {
            label: 'Total',
            amount: {
              currency: 'USD',
              value: '5.00',
            },
          },
        })
      .canMakePayment()
      .then(function(result) {
        print(result);
      })
      .catch(function(error) {
        print(error);
      });
  } catch (error) {
    print(error);
  }
}

/**
 * Merchant checks for ability to pay using "basic-card" with "visa" as the
 * supported network.
 */
function checkBasicVisa() { // eslint-disable-line no-unused-vars
  try {
    new PaymentRequest(
        [{
          supportedMethods: ['basic-card'],
          data: {
            supportedNetworks: ['visa'],
          },
        }], {
          total: {
            label: 'Total',
            amount: {
              currency: 'USD',
              value: '5.00',
            },
          },
        })
      .canMakePayment()
      .then(function(result) {
        print(result);
      })
      .catch(function(error) {
        print(error);
      });
  } catch (error) {
    print(error);
  }
}

/**
 * Merchant checks for ability to pay using "mastercard".
 */
function checkMasterCard() { // eslint-disable-line no-unused-vars
  try {
    new PaymentRequest(
        [{
          supportedMethods: ['mastercard'],
        }], {
          total: {
            label: 'Total',
            amount: {
              currency: 'USD',
              value: '5.00',
            },
          },
        })
      .canMakePayment()
      .then(function(result) {
        print(result);
      })
      .catch(function(error) {
        print(error);
      });
  } catch (error) {
    print(error);
  }
}

/**
 * Merchant checks for ability to pay using "visa".
 */
function checkVisa() { // eslint-disable-line no-unused-vars
  try {
    new PaymentRequest(
        [{
          supportedMethods: ['visa'],
        }], {
          total: {
            label: 'Total',
            amount: {
              currency: 'USD',
              value: '5.00',
            },
          },
        })
      .canMakePayment()
      .then(function(result) {
        print(result);
      })
      .catch(function(error) {
        print(error);
      });
  } catch (error) {
    print(error);
  }
}

/**
 * Merchant requests payment via either "mastercard" or "basic-card" with "visa"
 * as the supported network.
 */
function buy() { // eslint-disable-line no-unused-vars
  try {
    new PaymentRequest(
        [{
          supportedMethods: ['mastercard'],
        }, {
          supportedMethods: ['basic-card'],
          data: {
            supportedNetworks: ['visa'],
          },
        }], {
          total: {
            label: 'Total',
            amount: {
              currency: 'USD',
              value: '5.00',
            },
          },
        })
      .show()
      .then(function(response) {
        response.complete('success').then(function() {
          print(JSON.stringify(response, undefined, 2));
        }).catch(function(error) {
          print(error);
        });
      })
      .catch(function(error) {
        print(error);
      });
  } catch (error) {
    print(error);
  }
}

/**
 * Merchant requests payment via "basic-card" with "debit" as the supported card
 * type.
 */
function buyBasicDebit() { // eslint-disable-line no-unused-vars
  try {
    new PaymentRequest(
        [{
          supportedMethods: ['basic-card'],
          data: {
            supportedTypes: ['debit'],
          },
        }], {
          total: {
            label: 'Total',
            amount: {
              currency: 'USD',
              value: '5.00',
            },
          },
        })
      .show()
      .then(function(response) {
        response.complete('success').then(function() {
          print(JSON.stringify(response, undefined, 2));
        }).catch(function(error) {
          print(error);
        });
      })
      .catch(function(error) {
        print(error);
      });
  } catch (error) {
    print(error);
  }
}

/**
 * Merchant requests payment via "basic-card" payment method with "mastercard"
 * as the only supported network.
 */
function buyBasicMasterCard() { // eslint-disable-line no-unused-vars
  try {
    new PaymentRequest(
        [{
          supportedMethods: ['basic-card'],
          data: {
            supportedNetworks: ['mastercard'],
          },
        }], {
          total: {
            label: 'Total',
            amount: {
              currency: 'USD',
              value: '5.00',
            },
          },
        })
      .show()
      .then(function(response) {
        response.complete('success').then(function() {
          print(JSON.stringify(response, undefined, 2));
        }).catch(function(error) {
          print(error);
        });
      })
      .catch(function(error) {
        print(error);
      });
  } catch (error) {
    print(error);
  }
}
