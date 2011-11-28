// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Login UI header bar implementation.
 */

cr.define('login', function() {
  // Network state constants.
  const NET_STATE = {
    OFFLINE: 0,
    ONLINE: 1,
    PORTAL: 2
  };

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
                    ['login.HeaderBar.handleAddUser',
                     'check']);
      });
      $('cancel-add-user-button').addEventListener('click', function(e) {
        this.hidden = true;
        $('add-user-button').hidden = false;
        Oobe.showScreen({id: SCREEN_ACCOUNT_PICKER});
        Oobe.resetSigninUI();
      });
      $('sign-out-user-button').addEventListener('click', function(e) {
        chrome.send('signOutUser');
        this.disabled = true;
      });
    },

    /**
     * Tab index value for all button elements.
     * @type {number}
     */
    set buttonsTabIndex(tabIndex) {
      var buttons = this.getElementsByTagName('button');
      for (var i = 0, button; button = buttons[i]; ++i) {
        button.tabIndex = tabIndex;
      }
    },

    /**
     * Disables the header bar and all of its elements.
     * @type {boolean}
     */
    set disabled(value) {
      var buttons = this.getElementsByTagName('button');
      for (var i = 0, button; button = buttons[i]; ++i) {
        button.disabled = value;
      }
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
   * Continues add user button click handling after network state has
   * been recieved.
   * @param {Integer} state Current state of the network (see NET_STATE).
   * @param {string} network Name of the network.
   * @param {string} reason Reason the callback was called.
   */
  HeaderBar.handleAddUser = function(state, network, reason) {
    if (state != NET_STATE.OFFLINE) {
      Oobe.showSigninUI();
    } else {
      $('bubble').showTextForElement($('add-user-button'),
          localStrings.getString('addUserErrorMessage'));
      chrome.send('loginAddNetworkStateObserver',
                  ['login.HeaderBar.bubbleWatchdog']);
    }
  };

  /**
   * Observes network state, and close the bubble when network becomes online.
   * @param {Integer} state Current state of the network (see NET_STATE).
   * @param {string} network Name of the network.
   * @param {string} reason Reason the callback was called.
   */
  HeaderBar.bubbleWatchdog = function(state, network, reason) {
    if (state != NET_STATE.OFFLINE) {
      $('bubble').hideForElement($('add-user-button'));
      chrome.send('loginRemoveNetworkStateObserver',
                  ['login.HeaderBar.bubbleWatchdog']);
    }
  };

  return {
    HeaderBar: HeaderBar
  };
});
