// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview OOBE network selection screen implementation.
 */

login.createScreen('NetworkScreen', 'network-selection', function() {
  return {
    EXTERNAL_API: ['showError'],

    /** Dropdown element for networks selection. */
    dropdown_: null,

    /** @override */
    decorate: function() {
      var networkScreen = $('oobe-network-md');
      networkScreen.screen = this;
      networkScreen.enabled = true;
    },

    /** Returns a control which should receive an initial focus. */
    get defaultControl() {
      return $('oobe-network-md');
    },

    /**
     * Shows the network error message.
     * @param {string} message Message to be shown.
     */
    showError: function(message) {
      var error = document.createElement('div');
      var messageDiv = document.createElement('div');
      messageDiv.className = 'error-message-bubble';
      messageDiv.textContent = message;
      error.appendChild(messageDiv);
      error.setAttribute('role', 'alert');
    },

    /** This is called after resources are updated. */
    updateLocalizedContent: function() {
      $('oobe-network-md').updateLocalizedContent();
    },
  };
});
