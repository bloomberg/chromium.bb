// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design assistant
 * ready screen.
 *
 */

Polymer({
  is: 'assistant-ready',

  behaviors: [OobeDialogHostBehavior],

  /**
   * On-tap event handler for next button.
   *
   * @private
   */
  onNextTap_: function() {
    chrome.send('dialogClose');
  },

  /**
   * Signal from host to show the screen.
   */
  onShow: function() {
    this.$['next-button'].focus();
  },
});
