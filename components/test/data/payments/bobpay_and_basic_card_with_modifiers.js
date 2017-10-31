/*
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* global PaymentRequest:false */

/**
 * Launches the PaymentRequest UI with Bob Pay and basic-card as payment
 * methods and a modifier for basic-card.
 */
function buy() {  // eslint-disable-line no-unused-vars
  try {
    new PaymentRequest(
        [{supportedMethods: ['https://bobpay.com', 'basic-card']}], {
          total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
          modifiers: [{
            supportedMethods: ['basic-card'],
            total: {
              label: 'Total',
              amount: {currency: 'USD', value: '4.00'},
            },
            additionalDisplayItems: [{
              label: 'basic-card discount',
              amount: {currency: 'USD', value: '-1.00'},
            }],
            data: {discountProgramParticipantId: '86328764873265'},
          }],
        })
        .show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(JSON.stringify(resp, undefined, 2));
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

/**
 * Launches the PaymentRequest UI with Bob Pay and basic-card as payment
 * methods and a modifier for Bob Pay.
 */
function buyWithBobPayDiscount() {  // eslint-disable-line no-unused-vars
  try {
    new PaymentRequest(
        [{supportedMethods: ['https://bobpay.com', 'basic-card']}],
        {
          total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
          modifiers: [{
            supportedMethods: ['https://bobpay.com'],
            total: {
              label: 'Total',
              amount: {currency: 'USD', value: '4.00'},
            },
            additionalDisplayItems: [{
              label: 'BobPay discount',
              amount: {currency: 'USD', value: '-1.00'},
            }],
            data: {discountProgramParticipantId: '86328764873265'},
          }],
        })
        .show()
        .then(function(resp) {
          resp.complete('success')
            .then(function() {
              print(JSON.stringify(resp, undefined, 2));
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

/**
 * Launches the PaymentRequest UI with Bob Pay and basic-card as payment
 * methods and a modifier for basic-card with "credit" type
 */
function creditSupportedType() {  // eslint-disable-line no-unused-vars
  try {
    new PaymentRequest(
        [{supportedMethods: ['https://bobpay.com', 'basic-card']}], {
          total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
          modifiers: [{
            supportedMethods: ['basic-card'],
            total: {
              label: 'Total',
              amount: {currency: 'USD', value: '4.00'},
            },
            additionalDisplayItems: [{
              label: 'basic-card discount',
              amount: {currency: 'USD', value: '-1.00'},
            }],
            data: {
              discountProgramParticipantId: '86328764873265',
              supportedTypes: ['credit'],
            },
          }],
        })
        .show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(JSON.stringify(resp, undefined, 2));
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

/**
 * Launches the PaymentRequest UI with Bob Pay and basic-card as payment
 * methods and a modifier for basic-card with "debit" type
 */
function debitSupportedType() {  // eslint-disable-line no-unused-vars
  try {
    new PaymentRequest(
      [{supportedMethods: ['https://bobpay.com', 'basic-card']}], {
        total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
        modifiers: [{
          supportedMethods: ['basic-card'],
          total: {
            label: 'Total',
            amount: {currency: 'USD', value: '4.00'},
          },
          additionalDisplayItems: [{
            label: 'basic-card discount',
            amount: {currency: 'USD', value: '-1.00'},
          }],
          data: {
            discountProgramParticipantId: '86328764873265',
            supportedTypes: ['debit'],
          },
        }],
      })
      .show()
      .then(function(resp) {
        resp.complete('success')
        .then(function() {
          print(JSON.stringify(resp, undefined, 2));
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

/**
 * Launches the PaymentRequest UI with Bob Pay and basic-card as payment
 * methods and a modifier for basic-card with "credit" type and "visa" network
 */
function visaSupportedNetwork() {  // eslint-disable-line no-unused-vars
  try {
    new PaymentRequest(
        [{supportedMethods: ['https://bobpay.com', 'basic-card']}], {
          total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
          modifiers: [{
            supportedMethods: ['basic-card'],
            total: {
              label: 'Total',
              amount: {currency: 'USD', value: '4.00'},
            },
            additionalDisplayItems: [{
              label: 'basic-card discount',
              amount: {currency: 'USD', value: '-1.00'},
            }],
            data: {
              discountProgramParticipantId: '86328764873265',
              supportedTypes: ['credit'],
              supportedNetworks: ['visa'],
            },
          }],
        })
        .show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(JSON.stringify(resp, undefined, 2));
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

/**
 * Launches the PaymentRequest UI with Bob Pay and basic-card as payment
 * methods and a modifier for basic-card with "credit" type and " mastercard"
 * network
 */
function mastercardSupportedNetwork() {  // eslint-disable-line no-unused-vars
  try {
    new PaymentRequest(
        [{supportedMethods: ['https://bobpay.com', 'basic-card']}], {
          total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
          modifiers: [{
            supportedMethods: ['basic-card'],
            total: {
              label: 'Total',
              amount: {currency: 'USD', value: '4.00'},
            },
            additionalDisplayItems: [{
              label: 'basic-card discount',
              amount: {currency: 'USD', value: '-1.00'},
            }],
            data: {
              discountProgramParticipantId: '86328764873265',
              supportedTypes: ['credit'],
              supportedNetworks: ['mastercard'],
            },
          }],
        })
        .show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(JSON.stringify(resp, undefined, 2));
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

/**
 * Launches the PaymentRequest UI with Bob Pay and basic-card as payment
 * methods and a modifier for basic-card with "mastercard" network.
 */
function mastercardAnySupportedType() { // eslint-disable-line no-unused-vars
  try {
    new PaymentRequest(
        [{supportedMethods: ['https://bobpay.com', 'basic-card']}], {
          total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
          modifiers: [{
            supportedMethods: ['basic-card'],
            total: {
              label: 'Total',
              amount: {currency: 'USD', value: '4.00'},
            },
            additionalDisplayItems: [{
              label: 'basic-card discount',
              amount: {currency: 'USD', value: '-1.00'},
            }],
            data: {
              discountProgramParticipantId: '86328764873265',
              supportedNetworks: ['mastercard'],
            },
          }],
        })
        .show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(JSON.stringify(resp, undefined, 2));
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
