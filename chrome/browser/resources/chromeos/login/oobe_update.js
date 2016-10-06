// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Update screen.
 */

Polymer({
  is: 'oobe-update-md',

  properties: {
    /**
     * Shows "Checking for update ..." section and hides "Updating..." section.
     */
    checkingForUpdate: {
      type: Boolean,
      value: true,
    },

    /**
     * Progress bar percent.
     */
    progressValue: {
      type: Number,
      value: 0,
    },

    /**
     * Message "3 minutes left".
     */
    estimatedTimeLeft: {
      type: String,
    },

    /**
     * Shows estimatedTimeLeft.
     */
    estimatedTimeLeftShown: {
      type: Boolean,
    },

    /**
     * Message "33 percent done".
     */
    progressMessage: {
      type: String,
    },

    /**
     * Shows progressMessage.
     */
    progressMessageShown: {
      type: Boolean,
    },

    /**
     * Message above the progress bar.
     */
    updateUpperLabel: {
      type: String,
    },

    /**
     * If update cancellation is allowed.
     */
    cancelAllowed: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * This updates "Cancel Update" message.
   */
  setCancelHint: function(message) {
    this.$.checkingForUpdateCancelHint.textContent = message;
    this.$.updatingCancelHint.textContent = message;
  },
});
