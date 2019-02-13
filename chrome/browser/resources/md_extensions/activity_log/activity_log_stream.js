// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  const ActivityLogStream = Polymer({
    is: 'activity-log-stream',

    properties: {
      /** @private */
      isStreamOn_: {
        type: Boolean,
        value: false,
      },
    },

    /** @override */
    attached: function() {
      this.isStreamOn_ = true;
    },

    /** @override */
    detached: function() {
      this.isStreamOn_ = false;
    },

    /** @return {boolean} */
    isStreamOnForTest() {
      // TODO(kelvinjiang): remove this method when the event listener has been
      // hooked up to this component (in the CL after this one). At that time,
      // onExtensionActivity.hasListeners() will be used instead.
      return this.isStreamOn_;
    },

    /** @private */
    onToggleButtonTap_: function() {
      this.isStreamOn_ = !this.isStreamOn_;
    },
  });

  return {
    ActivityLogStream: ActivityLogStream,
  };
});
