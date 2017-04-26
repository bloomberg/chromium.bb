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

    isRunning: {
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
    this.isRunningScanner = true;
    this.browserProxy_.startScan().then(this.onScanComplete_.bind(this));
  },

  /**
   * @param {LastScanResult} lastScanResults
   */
  onScanComplete_: function(lastScanResults) {
    this.isRunningScanner = false;
    this.updateLastScanState_(lastScanResults);
  },

  /**
   * Sends a request for Chrome to open the Cleanup Tool's cleanup prompt.
   * @private
   */
  onCleanupTap_: function() {
    this.isRunning = true;
    this.browserProxy_.startCleanup().then(this.onCleanupResult_.bind(this));
  },

  /**
   * @param {CleanupResult} cleanupResults
   */
  onCleanupResult_: function(cleanupResults) {
    this.isRunning = false;

    if (!cleanupResults.wasCancelled)
      // Assume cleanup was completed and clear infected status.
      this.isInfected = false;

    // TODO(proberge): Do something about cleanupResults.uwsRemoved.
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
   * @param {boolean} isRunning
   * @return {boolean} Whether the "Scan" button should be displayed.
   * @private
   */
  shouldShowScan_: function(hasScanResults, isRunning) {
    return !hasScanResults && !isRunning;
  },

  /**
   * @param {boolean} hasScanResults
   * @param {boolean} isRunning
   * @return {boolean} Whether the "Run Chrome Cleanup" button should be
   *     displayed.
   * @private
   */
  shouldShowClean_: function(hasScanResults, isRunning) {
    return hasScanResults && !isRunning;
  },

  /**
   * @param {boolean} hasScanResults
   * @param {boolean} isRunning
   * @return {boolean} True Whether the "Scanning" label should be displayed.
   * @private
   */
  shouldShowScanning_: function(hasScanResults, isRunning) {
    return !hasScanResults && isRunning;
  },

  /**
   * @param {boolean} hasScanResults
   * @param {boolean} isRunning
   * @return {boolean} True Whether the "Cleaning" label should be displayed.
   * @private
   */
  shouldShowCleaning_: function(hasScanResults, isRunning) {
    return hasScanResults && isRunning;
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
