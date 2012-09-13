// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Object used to measure usage statistics.
   * @constructor
   */
  function Metrics() {};

  /**
   * Enumeration of metrics buckets to record.
   * @enum {number}
   */
  Metrics.Bucket = {
    // Used when the print destination search widget is shown.
    DESTINATION_SEARCH_SHOWN: 0,
    // Used when the user selects a print destination.
    DESTINATION_SELECTED: 1,
    // Used when the print destination search widget is closed without selecting
    // a print destination.
    DESTINATION_SELECTION_CANCELED: 2,
    // Used when the Google Cloud Print promotions is shown to the user.
    CLOUDPRINT_PROMO_SHOWN: 3,
    // Used when the user chooses to sign-in to their Google account.
    SIGNIN_TRIGGERED: 4
  };

  Metrics.prototype = {
    /**
     * Increments the counter for a given bucket.
     * @param {!print_preview.Metrics.Bucket} bucket Bucket to increment.
     */
    increment: function(bucket) {
      chrome.send('reportDestinationEvent', [bucket]);
    }
  };

  // Export
  return {
    Metrics: Metrics
  };
});
