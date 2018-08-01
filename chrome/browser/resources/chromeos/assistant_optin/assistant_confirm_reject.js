// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design assistant
 * confirm reject screen.
 *
 * Event 'loading' will be fired when the page is loading/reloading.
 * Event 'loaded' will be fired when the page has been successfully loaded.
 */

Polymer({
  is: 'assistant-confirm-reject',

  behaviors: [OobeDialogHostBehavior],

  properties: {
    /**
     * Buttons are disabled when the page content is loading.
     */
    buttonsDisabled: {
      type: Boolean,
      value: true,
    },
  },

  /**
   * Whether all the screen content has been successfully loaded.
   * @type {boolean}
   * @private
   */
  pageLoaded_: false,

  /**
   * On-tap event handler for next button.
   *
   * @private
   */
  onNextTap_: function() {
    var confirmResult = this.$['accept'].getAttribute('checked') != null;
    chrome.send('AssistantConfirmRejectScreen.userActed', [confirmResult]);
  },

  /**
   * Reloads the page.
   */
  reloadPage: function() {
    this.fire('loading');
    this.buttonsDisabled = true;
  },

  /**
   * Reload the page with the given consent string text data.
   */
  reloadContent: function(data) {
    this.$['title-text'].textContent = data['confirmRejectTitle'];
    this.$['next-button-text'].textContent =
        data['confirmRejectContinueButton'];
    this.$['accept-title-text'].textContent = data['confirmRejectAcceptTitle'];
    this.$['accept-message-text'].textContent =
        data['confirmRejectAcceptMessage'];
    this.$['accept-message-extra-text'].textContent =
        data['confirmRejectAcceptMessageExpanded'];
    this.$['reject-title-text'].textContent = data['confirmRejectRejectTitle'];
    this.$['reject-message-text'].textContent =
        data['confirmRejectRejectMessage'];

    this.pageLoaded_ = true;
    this.onPageLoaded();
  },

  /**
   * Handles event when all the page content has been loaded.
   */
  onPageLoaded: function() {
    this.fire('loaded');
    this.buttonsDisabled = false;
    this.$['next-button'].focus();
  },

  /**
   * Signal from host to show the screen.
   */
  onShow: function() {
    if (!this.pageLoaded_) {
      this.reloadPage();
    } else {
      this.$['next-button'].focus();
    }
  },
});
