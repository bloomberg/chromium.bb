// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-destination-settings',

  properties: {
    /** @type {!print_preview.Destination} */
    destination: Object,

    /** @private {boolean} */
    loadingDestination_: Boolean,
  },

  /** @override */
  ready: function() {
    this.loadingDestination_ = true;
    // Simulate transition from spinner to destination.
    setTimeout(this.doneLoading_.bind(this), 5000);
  },

  /** @private */
  doneLoading_: function() {
    this.loadingDestination_ = false;
  },
});
