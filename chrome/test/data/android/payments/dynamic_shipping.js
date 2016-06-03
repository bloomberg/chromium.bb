/*
 * Copyright 2016 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* global PaymentRequest:false */
/* global toDictionary:false */

/**
 * Launches the PaymentRequest UI that offers free shipping in California and
 * $5.00 shipping in US. Does not allow shipping outside of US.
 */
function buy() {  // eslint-disable-line no-unused-vars
  try {
    var details = {
      displayItems: [
        {
          id: 'sub',
          label: 'Subtotal',
          amount: {currency: 'USD', value: '5.00'}
        },
        {
          id: 'total',
          label: 'Total',
          amount: {currency: 'USD', value: '5.00'}
        }
      ]
    };

    var request =
        new PaymentRequest(['visa'], details, {requestShipping: true});

    request.addEventListener('shippingaddresschange', function(evt) {
      evt.updateWith(new Promise(function(resolve) {
        resolve(updateDetails(details, request.shippingAddress));
      }));
    });

    request.show()
        .then(function(resp) {
          resp.complete(true)
              .then(function() {
                print(request.shippingOption + '<br>' +
                    JSON.stringify(
                        toDictionary(request.shippingAddress), undefined, 2) +
                    '<br>' + resp.methodName + '<br>' +
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

/**
 * Updates the shopping cart with the appropriate shipping prices according to
 * the shipping address.
 * @param {object} details - The shopping cart.
 * @param {ShippingAddress} addr - The shipping address.
 * @return {object} The updated shopping cart.
 */
function updateDetails(details, addr) {
  if (addr.regionCode === 'US') {
    var shippingOption = {
      id: '',
      label: '',
      amount: {currency: 'USD', value: '0.00'}
    };
    if (addr.administrativeArea === 'CA') {
      shippingOption.id = 'ca';
      shippingOption.label = 'Free shipping in California';
      details.displayItems[details.displayItems.length - 1].amount.value =
          '5.00';
    } else {
      shippingOption.id = 'us';
      shippingOption.label = 'Standard shipping in US';
      shippingOption.amount.value = '5.00';
      details.items[details.items.length - 1].amount.value = '10.00';
    }
    if (details.items.length === 3) {
      details.items.splice(-1, 0, shippingOption);
    } else {
      details.items.splice(-2, 1, shippingOption);
    }
    details.shippingOptions = [shippingOption];
  } else {
    delete details.shippingOptions;
  }
  return details;
}
