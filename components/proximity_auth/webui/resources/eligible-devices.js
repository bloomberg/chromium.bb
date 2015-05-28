// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer('eligible-devices', {
  /**
   * List of devices that are eligible to be used as an unlock key.
   * @type {Array<DeviceInfo>}
   * @private
   */
  eligibleDevices_: null,

  /**
   * List of devices that are ineligible to be used as an unlock key.
   * @type {Array<DeviceInfo>}
   * @private
   */
  ineligibleDevices_: null,

  /**
   * Whether the findEligibleUnlockDevices request is in progress.
   * @type {boolean}
   * @private
   */
  requestInProgress_: false,

  /**
   * Called when this element is added to the DOM
   */
  attached: function() {
    this.eligibleDevices_ = [];
    this.ineligibleDevices_ = [];
    CryptAuthInterface.addObserver(this);
  },

  /**
   * Called when this element is removed from the DOM.
   */
  detatched: function() {
    CryptAuthInterface.removeObserver(this);
  },

  /**
   * Called when the page is about to be shown.
   */
  activate: function() {
    this.requestInProgress_ = true;
    CryptAuthInterface.findEligibleUnlockDevices();
  },

  /**
   * Called when eligible devices are found.
   * @param {Array.<EligibleDevice>} eligibleDevices
   * @param {Array.<IneligibleDevice>} ineligibleDevices_
   */
  onGotEligibleDevices: function(eligibleDevices, ineligibleDevices) {
    this.requestInProgress_ = false;
    this.eligibleDevices_ = eligibleDevices;
    this.ineligibleDevices_ = ineligibleDevices;
  },

  /**
   * Called when the CryptAuth request fails.
   * @param {string} errorMessage
   */
  onCryptAuthError: function(errorMessage) {
    console.error('CryptAuth request failed: ' + errorMessage);
    this.requestInProgress_ = false;
  },
});
