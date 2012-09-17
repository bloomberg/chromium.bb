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
   * Enumeration of metrics bucket groups. Each group describes a set of events
   * that can happen in order. This implies that an event cannot occur twice and
   * an event that occurs semantically before another event, should not occur
   * after.
   * @enum {number}
   */
  Metrics.BucketGroup = {
    DESTINATION_SEARCH: 0
  };

  /**
   * Enumeration of buckets that a user can enter while using the destination
   * search widget.
   * @enum {number}
   */
  Metrics.DestinationSearchBucket = {
    // Used when the print destination search widget is shown.
    SHOWN: 0,
    // Used when the user selects a print destination.
    DESTINATION_SELECTED: 1,
    // Used when the print destination search widget is closed without selecting
    // a print destination.
    CANCELED: 2,
    // Used when the Google Cloud Print promotion (shown in the destination
    // search widget) is shown to the user.
    CLOUDPRINT_PROMO_SHOWN: 3,
    // Used when the user chooses to sign-in to their Google account.
    SIGNIN_TRIGGERED: 4
  };

  /**
   * Name of the C++ function to call to increment bucket counts.
   * @type {string}
   * @const
   * @private
   */
  Metrics.NATIVE_FUNCTION_NAME_ = 'reportUiEvent';

  Metrics.prototype = {
    /**
     * Increments the counter of a destination-search bucket.
     * @param {!Metrics.DestinationSearchBucket} bucket Bucket to increment.
     */
    incrementDestinationSearchBucket: function(bucket) {
      chrome.send(Metrics.NATIVE_FUNCTION_NAME_,
                  [Metrics.BucketGroup.DESTINATION_SEARCH, bucket]);
    }
  };

  // Export
  return {
    Metrics: Metrics
  };
});
