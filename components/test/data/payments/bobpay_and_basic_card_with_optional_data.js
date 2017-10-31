/*
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Launches the PaymentRequest UI with Bob Pay as payment method.
 */
 function buy() {  // eslint-disable-line no-unused-vars
  try {
    new PaymentRequest(
        [{supportedMethods: ['https://bobpay.com']}],
        {
          total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
        })
        .show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(JSON.stringify(resp, undefined, 2));
              })
              .catch(function(error) {
                print('complete() rejected<br>' + error.message);
              });
        })
        .catch(function(error) {
          print('show() rejected<br>' + error.message);
        });
  } catch (error) {
    print('exception thrown<br>' + error.message);
  }
}

/**
 * Launches the PaymentRequest UI with all cards as payment methods.
 */
 function buyWithAllCards() {  // eslint-disable-line no-unused-vars
  try {
    new PaymentRequest(
        [{supportedMethods: ['basic-card']}],
        {
          total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
        })
        .show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(JSON.stringify(resp, undefined, 2));
              })
              .catch(function(error) {
                print('complete() rejected<br>' + error.message);
              });
        })
        .catch(function(error) {
          print('show() rejected<br>' + error.message);
        });
  } catch (error) {
    print('exception thrown<br>' + error.message);
  }
}

/**
 * Launches the PaymentRequest UI with visa credit card as payment method.
 */
 function buyWithVisaCredit() {  // eslint-disable-line no-unused-vars
  try {
    new PaymentRequest(
        [{
          supportedMethods: ['basic-card'],
          data: {
            supportedTypes: ['credit'],
            supportedNetworks: ['visa'],
          },
        }],
        {
          total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
        })
      .show()
      .then(function(resp) {
        resp.complete('success')
          .then(function() {
            print(JSON.stringify(resp, undefined, 2));
          })
          .catch(function(error) {
            print('complete() rejected<br>' + error.message);
          });
        })
      .catch(function(error) {
        print('show() rejected<br>' + error.message);
      });
    } catch (error) {
      print('exception thrown<br>' + error.message);
    }
  }

/**
 * Launches the PaymentRequest UI with visa debit card as payment method.
 */
 function buyWithVisaDebit() {  // eslint-disable-line no-unused-vars
  try {
    new PaymentRequest(
        [{
          supportedMethods: ['basic-card'],
          data: {
            supportedTypes: ['debit'],
            supportedNetworks: ['visa'],
          },
        }],
        {
          total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
        })
      .show()
      .then(function(resp) {
        resp.complete('success')
          .then(function() {
            print(JSON.stringify(resp, undefined, 2));
          })
          .catch(function(error) {
            print('complete() rejected<br>' + error.message);
          });
        })
      .catch(function(error) {
        print('show() rejected<br>' + error.message);
      });
    } catch (error) {
      print('exception thrown<br>' + error.message);
    }
 }

/**
 * Launches the PaymentRequest UI with credit card as payment method.
 */
 function buyWithCredit() {  // eslint-disable-line no-unused-vars
  try {
    new PaymentRequest(
      [{
        supportedMethods: ['basic-card'],
        data: {
          supportedTypes: ['credit'],
        },
      }],
      {
        total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
      })
      .show()
      .then(function(resp) {
        resp.complete('success')
          .then(function() {
            print(JSON.stringify(resp, undefined, 2));
          })
          .catch(function(error) {
            print('complete() rejected<br>' + error.message);
          });
        })
      .catch(function(error) {
        print('show() rejected<br>' + error.message);
      });
    } catch (error) {
      print('exception thrown<br>' + error.message);
    }
 }

/**
 * Launches the PaymentRequest UI with visa card as payment method.
 */
 function buyWithVisa() {  // eslint-disable-line no-unused-vars
  try {
    new PaymentRequest(
        [{
          supportedMethods: ['basic-card'],
          data: {
            supportedNetworks: ['visa'],
          },
        }],
        {
          total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
        })
      .show()
      .then(function(resp) {
        resp.complete('success')
          .then(function() {
            print(JSON.stringify(resp, undefined, 2));
          })
          .catch(function(error) {
            print('complete() rejected<br>' + error.message);
          });
        })
      .catch(function(error) {
        print('show() rejected<br>' + error.message);
      });
    } catch (error) {
      print('exception thrown<br>' + error.message);
    }
 }

 /**
 * Launches the PaymentRequest UI with Bob Pay and visa card as payment methods.
 */
 function buyWithBobPayAndVisa() {  // eslint-disable-line no-unused-vars
  try {
    new PaymentRequest(
        [{
          supportedMethods: ['https://bobpay.com', 'basic-card'],
          data: {
            supportedNetworks: ['visa'],
          },
        }],
        {
          total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
        })
      .show()
      .then(function(resp) {
        resp.complete('success')
          .then(function() {
            print(JSON.stringify(resp, undefined, 2));
          })
          .catch(function(error) {
            print('complete() rejected<br>' + error.message);
          });
        })
      .catch(function(error) {
        print('show() rejected<br>' + error.message);
      });
    } catch (error) {
      print('exception thrown<br>' + error.message);
    }
 }
