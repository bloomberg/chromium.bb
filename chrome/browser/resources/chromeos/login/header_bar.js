// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Login UI header bar implementation.
 */

cr.define('login', function() {

  /**
   * Creates a header bar element.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var HeaderBar = cr.ui.define('div');

  HeaderBar.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @inheritDoc */
    decorate: function() {
      $('shutdown-header-bar-item').addEventListener('click',
          this.handleShutdownClick_);
      $('shutdown-button').addEventListener('click',
          this.handleShutdownClick_);
      $('add-user-button').addEventListener('click', function(e) {
        chrome.send('loginRequestNetworkState',
                    ['login.HeaderBar.updateState']);
      });
      $('cancel-add-user-button').addEventListener('click', function(e) {
        this.hidden = true;
        $('add-user-button').hidden = false;
        Oobe.showScreen({id: SCREEN_ACCOUNT_PICKER});
        Oobe.resetSigninUI();
      });
    },

    /**
     * Shutdown button click handler.
     * @private
     */
    handleShutdownClick_: function(e) {
      chrome.send('shutdownSystem');
    }
  };

  /**
   * Network state changed callback.
   * @param {Integer} state Current state of the network: 0 - offline;
   * 1 - online; 2 - under the captive portal.
   */
  HeaderBar.updateState = function(state) {
    var isOffline = state == 0;
    if (!isOffline) {
      Oobe.showSigninUI();
    } else {
      $('bubble').showTextForElement($('add-user-button'),
          localStrings.getString('addUserOfflineMessage'));
    }
  }

  return {
    HeaderBar: HeaderBar
  };
});
