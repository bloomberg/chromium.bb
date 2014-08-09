// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Implements a check whether an origin is allowed to assert an
 * app id based on a fixed set of allowed app ids for the google.com domain.
 *
 */
'use strict';

/**
 * Implements half of the app id policy: whether an origin is allowed to claim
 * an app id. For checking whether the app id also lists the origin,
 * @see AppIdChecker.
 * @implements OriginChecker
 * @constructor
 */
function GstaticOriginChecker() {
}

/**
 * Checks whether the origin is allowed to claim the app ids.
 * @param {string} origin The origin claiming the app id.
 * @param {!Array.<string>} appIds The app ids being claimed.
 * @return {Promise.<boolean>} A promise for the result of the check.
 */
GstaticOriginChecker.prototype.canClaimAppIds = function(origin, appIds) {
  return Promise.resolve(appIds.every(this.checkAppId_.bind(this, origin)));
};

/**
 * Checks if a single appId can be asserted by the given origin.
 * @param {string} origin The origin.
 * @param {string} appId The appId to check.
 * @return {boolean} Whether the given origin can assert the app id.
 * @private
 */
GstaticOriginChecker.prototype.checkAppId_ = function(origin, appId) {
  if (appId == origin) {
    // Trivially allowed
    return true;
  }
  if (/google.com$/.test(origin)) {
    return (appId.indexOf('https://www.gstatic.com') == 0 ||
        appId.indexOf('https://static.corp.google.com') == 0);
  }
  return false;
};
