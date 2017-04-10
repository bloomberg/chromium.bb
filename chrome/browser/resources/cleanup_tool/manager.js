// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'cleanup-manager',

  properties: {
    hasScanResults: {
      type: Boolean,
      value: false,
    },

    isInfected: {
      type: Boolean,
      value: false,
    },

    isRunningScanner: {
      type: Boolean,
      value: false,
    },

    detectionStatusText: {
      type: String,
      value: ""
    },

    detectionTimeText: {
      type: String,
      value: ""
    },

    /** @private {!cleanup.CleanupBrowserProxy} */
    browserProxy_: Object,
  },

  /** @override */
  attached: function() {
    // Fetch data to have it displayed as soon as possible.
    this.requestLastScanResult_();
  },

  /** @override */
  created: function() {
    this.browserProxy_ = cleanup.CleanupBrowserProxyImpl.getInstance();
  },

  /**
   * Sends a request for Chrome to start the Cleanup Tool's scanning process.
   * @private
   */
  onScanTap_: function() {
    // TODO implement me.
  },

  /**
   * Sends a request for Chrome to open the Cleanup Tool's cleanup prompt.
   * @private
   */
  onCleanupTap_: function() {
    // TODO implement me.
  },

  /**
   * @param {boolean} hasScanResults
   * @param {boolean} isInfected
   * @return {string} A class name for icon-specific styling.
   * @private
   */
  getIconClassName_: function(hasScanResults, isInfected) {
    return hasScanResults && isInfected ? "infected-icon" : "clean-icon";
  },

  /**
   * @param {boolean} hasScanResults
   * @param {boolean} isInfected
   * @return {string} An icon id. See icons.html.
   * @private
   */
  getStatusIcon_: function(hasScanResults, isInfected) {
    return hasScanResults && isInfected ?
        "cleanup:infected-user" :
        "cleanup:verified-user";
  },

  /**
   * @param {boolean} hasScanResults
   * @param {boolean} isRunningScanner
   * @return {boolean} True if the "Scan" button should be displayed, false
   *     otherwise.
   * @private
   */
  shouldShowScan_: function(hasScanResults, isRunningScanner) {
    return !hasScanResults && !isRunningScanner;
  },

  /**
   * Requests the latest Chrome Cleanup Tool scan results from Chrome, then
   * updates the local state with the new information.
   * @private
   */
  requestLastScanResult_: function() {
    this.browserProxy_.requestLastScanResult().then(
        this.updateLastScanState_.bind(this));
  },

  /**
   @param {LastScanResult} lastScanResults
  */
  updateLastScanState_: function(lastScanResults) {
    this.hasScanResults = lastScanResults.hasScanResults;
    this.isInfected = lastScanResults.hasScanResults;
    this.detectionStatusText = lastScanResults.detectionStatusText;
    this.detectionTimeText = lastScanResults.detectionTimeText;
  }
});
