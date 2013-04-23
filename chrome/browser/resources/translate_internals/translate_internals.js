// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  'use strict';

  cr.define('cr.translateInternals', function() {

    /**
     * Initializes UI and sends a message to the browser for
     * initialization.
     */
    function initialize() {
      cr.ui.decorate('tabbox', cr.ui.TabBox);
      chrome.send('requestInfo');
    }

    /**
     * Handles the message of 'prefsUpdated' from the browser.
     * @param {Object} detail The object which represents pref values.
     */
    function onPrefsUpdated(detail) {
      var p = $('tabpanel-prefs').querySelector('p');
      var content = JSON.stringify(detail, null, 2);
      p.textContent = content;
    }

    /**
     * The callback entry point from the browser. This function will be
     * called by the browser.
     * @param {string} message The name of the sent message
     * @param {Object} detail The argument of the sent message
     */
    function messageHandler(message, detail) {
      switch (message) {
        case 'prefsUpdated':
          cr.translateInternals.onPrefsUpdated(detail);
          break;
        default:
          console.error('Unknown message:', message);
          break;
      }
    }

    return {
      initialize: initialize,
      messageHandler: messageHandler,
      onPrefsUpdated: onPrefsUpdated
    };
  });

  /**
   * The entry point of the UI.
   */
  function main() {
    cr.doc.addEventListener('DOMContentLoaded',
                            cr.translateInternals.initialize);
  }

  main();
})();
