// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides an implementation of approved origins that relies
 * on the chrome.cryptotokenPrivate.requestPermission API.
 * (and only) allows google.com to use security keys.
 *
 */
'use strict';

/**
 * Allows the caller to check whether the user has approved the use of
 * security keys from an origin.
 * @constructor
 * @implements {ApprovedOrigins}
 */
function CryptoTokenApprovedOrigin() {}

/**
 * Checks whether the origin is approved to use security keys. (If not, an
 * approval prompt may be shown.)
 * @param {string} origin The origin to approve.
 * @param {number=} opt_tabId A tab id to display approval prompt in.
 *     For this implementation, the tabId is always necessary, even though
 *     the type allows undefined.
 * @return {Promise.<boolean>} A promise for the result of the check.
 */
CryptoTokenApprovedOrigin.prototype.isApprovedOrigin =
    function(origin, opt_tabId) {
  return new Promise(function(resolve, reject) {
      if (!chrome.cryptotokenPrivate ||
          !chrome.cryptotokenPrivate.requestPermission) {
        resolve(false);
        return;
      }
      if (!opt_tabId) {
        resolve(false);
        return;
      }
      var tabId = /** @type {number} */ (opt_tabId);
      chrome.cryptotokenPrivate.requestPermission(tabId, origin,
          function(result) {
            if (result == 'ALLOWED') {
              resolve(true);
              return;
            }
            if (chrome.runtime.lastError) {
              console.log(UTIL_fmt('lastError: ' + chrome.runtime.lastError));
            }
            resolve(false);
          });
  });
};
