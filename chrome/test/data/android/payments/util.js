/*
 * Copyright 2016 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Prints the message.
 * @param {String} msg - The message to print.
 */
function print(msg) {  // eslint-disable-line no-unused-vars
  document.getElementById('result').innerHTML = msg;
}

/**
 * Converts ShippingAddress into a dictionary.
 * @param {ShippingAddress} addr - The object to convert.
 * @return {object} The dictionary output.
 */
function toDictionary(addr) {  // eslint-disable-line no-unused-vars
  var dict = {};
  if (addr) {
    dict.country = addr.country;
    dict.region = addr.region;
    dict.city = addr.city;
    dict.dependentLocality = addr.dependentLocality;
    dict.addressLine = addr.addressLine;
    dict.postalCode = addr.postalCode;
    dict.sortingCode = addr.sortingCode;
    dict.languageCode = addr.languageCode;
    dict.organization = addr.organization;
    dict.recipient = addr.recipient;
    dict.careOf = addr.careOf;
    dict.phone = addr.phone;
  }
  return dict;
}
