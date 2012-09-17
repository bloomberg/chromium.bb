// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Login UI header bar implementation.
 */

cr.define('login', function() {
  // Network state constants.
  /** @const */ var NET_STATE = {
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
        Oobe.resetSigninUI(true);
      });
      $('sign-out-user-button').addEventListener('click', function(e) {
        this.disabled = true;
        chrome.send('signOutUser');
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
      e.stopPropagation();
    }
  };

  /**
   * Continues add user button click handling after network state has
   * been recieved.
   * @param {number} state Current state of the network (see NET_STATE).
   * @param {string} network Name of the network.
   * @param {string} reason Reason the callback was called.
   * @param {number} last Last active network type.
   */
  HeaderBar.handleAddUser = function(state, network, reason, last) {
    if (state != NET_STATE.OFFLINE) {
      Oobe.showSigninUI();
    } else {
      /** @const */ var BUBBLE_OFFSET = 8;
      /** @const */ var BUBBLE_PADDING = 5;
      $('bubble').showTextForElement(
          $('add-user-button'),
          localStrings.getString('addUserErrorMessage'),
          cr.ui.Bubble.Attachment.TOP, BUBBLE_OFFSET, BUBBLE_PADDING);
      chrome.send('loginAddNetworkStateObserver',
                  ['login.HeaderBar.bubbleWatchdog']);
    }
  };

  /**
   * Observes network state, and close the bubble when network becomes online.
   * @param {number} state Current state of the network (see NET_STATE).
   * @param {string} network Name of the network.
   * @param {string} reason Reason the callback was called.
   * @param {number} last Last active network type.
   */
  HeaderBar.bubbleWatchdog = function(state, network, reason, last) {
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
