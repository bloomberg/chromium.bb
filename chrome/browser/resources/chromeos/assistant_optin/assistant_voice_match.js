// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design assistant
 * voice match screen.
 *
 */
Polymer({
  is: 'assistant-voice-match',

  behaviors: [OobeDialogHostBehavior],

  /**
   * Current recording index.
   * @type {number}
   * @private
   */
  currentIndex_: 0,

  /**
   * On-tap event handler for skip button.
   *
   * @private
   */
  onSkipTap_: function() {
    chrome.send(
        'login.AssistantOptInFlowScreen.VoiceMatchScreen.userActed',
        ['skip-pressed']);
  },

  /**
   * On-tap event handler for agree button.
   *
   * @private
   */
  onAgreeTap_: function() {
    this.removeClass_('intro');
    this.addClass_('recording');
    chrome.send(
        'login.AssistantOptInFlowScreen.VoiceMatchScreen.userActed',
        ['record-pressed']);
  },

  /**
   * Add class to the list of classes of root elements.
   * @param {string} className class to add
   *
   * @private
   */
  addClass_: function(className) {
    this.$['voice-match-dialog'].classList.add(className);
  },

  /**
   * Remove class to the list of classes of root elements.
   * @param {string} className class to remove
   *
   * @private
   */
  removeClass_: function(className) {
    this.$['voice-match-dialog'].classList.remove(className);
  },

  /**
   * Called when the server is ready to listening for hotword.
   */
  listenForHotword: function() {
    var currentEntry = this.$['voice-entry-' + this.currentIndex_];
    currentEntry.setAttribute('active', true);
  },

  /**
   * Called when the server has detected and processing hotword.
   */
  processingHotword: function() {
    var currentEntry = this.$['voice-entry-' + this.currentIndex_];
    currentEntry.removeAttribute('active');
    currentEntry.setAttribute('completed', true);
    this.currentIndex_++;
    if (this.currentIndex_ == 4) {
      this.$['voice-match-entries'].hidden = true;
      this.$['later-button'].hidden = true;
      this.$['loading-animation'].hidden = false;
    }
  },

  voiceMatchDone: function() {
    this.removeClass_('recording');
    this.addClass_('completed');

    window.setTimeout(function() {
      chrome.send(
          'login.AssistantOptInFlowScreen.VoiceMatchScreen.userActed',
          ['voice-match-done']);
    }, 2000);
  },

  /**
   * Signal from host to show the screen.
   */
  onShow: function() {
    this.$['agree-button'].focus();
  },
});
