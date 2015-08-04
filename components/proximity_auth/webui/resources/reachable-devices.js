// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'reachable-devices',

  properties: {
    /**
     * List of devices that recently responded to a CryptAuth ping.
     * @type {Array<DeviceInfo>}
     * @private
     */
    reachableDevices_: {
      type: Array,
      value: null,
    },

    /**
     * Whether the findEligibleUnlockDevices request is in progress.
     * @type {boolean}
     * @private
     */
    requestInProgress_: Boolean,
  },

  /**
   * Called when this element is added to the DOM.
   */
  attached: function() {
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
    this.reachableDevices_ = null;
    CryptAuthInterface.findReachableDevices();
  },

  /**
   * Called when reachable devices are found.
   * @param {Array<EligibleDevice>} reachableDevices
   */
  onGotReachableDevices: function(reachableDevices) {
    this.requestInProgress_ = false;
    this.reachableDevices_ = reachableDevices;
  },

  /**
   * Called when the CryptAuth request fails.
   * @param {string} errorMessage
   */
  onCryptAuthError: function(errorMessage) {
    console.error('CryptAuth request failed: ' + errorMessage);
    this.requestInProgress_ = false;
    this.reachableDevices_ = null;
  },
});
