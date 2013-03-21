// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Kiosk apps menu implementation.
 */

cr.define('login', function() {
  'use strict';

  var Menu = cr.ui.Menu;
  var MenuButton = cr.ui.MenuButton;

  /**
   * Creates apps menu button.
   * @constructor
   * @extends {cr.ui.MenuButton}
   */
  var AppsMenuButton = cr.ui.define('button');

  AppsMenuButton.prototype = {
    __proto__: MenuButton.prototype,

    /**
     * Flag of whether to rebuild the menu.
     * @type {boolean}
     * @private
     */
    needsRebuild_: true,

    /**
     * Array to hold apps info.
     * @type {Array}
     */
    data_: null,
    get data() {
      return this.data_;
    },
    set data(data) {
      this.data_ = data;
      this.needsRebuild_ = true;
    },

    /** @override */
    decorate: function() {
      MenuButton.prototype.decorate.call(this);
      this.menu = new Menu;
      cr.ui.decorate(this.menu, Menu);
      document.body.appendChild(this.menu);

      this.anchorType = cr.ui.AnchorType.ABOVE;
      chrome.send('initializeKioskApps');
    },

    /** @override */
    showMenu: function(shouldSetFocus) {
      if (this.needsRebuild_) {
        this.menu.textContent = '';
        this.data_.forEach(this.addItem_, this);
        this.needsRebuild_ = false;
      }

      MenuButton.prototype.showMenu.apply(this, arguments);
    },

    /**
     * Invoked when apps menu becomes visible.
     */
    didShow: function() {
      window.setTimeout(function() {
        if (!$('apps-header-bar-item').hidden)
          chrome.send('checkKioskAppLaunchError');
      }, 500);
    },

    /**
     * Adds an app to the menu.
     * @param {Object} app An app info object.
     * @private
     */
    addItem_: function(app) {
      var menuItem = this.menu.addMenuItem(app);
      menuItem.classList.add('apps-menu-item');
      menuItem.addEventListener('activate', function() {
        chrome.send('launchKioskApp', [app.id]);
      });
    }
  };

  /**
   * Sets apps to be displayed in the apps menu.
   * @param {!Array.<!Object>} apps An array of app info objects.
   */
  AppsMenuButton.setApps = function(apps) {
    $('show-apps-button').data = apps;
    $('login-header-bar').hasApps = apps.length > 0;
  };

  /**
   * Shows the given error message.
   * @param {!string} message Error message to show.
   */
  AppsMenuButton.showError = function(message) {
    /** @const */ var BUBBLE_OFFSET = 25;
    /** @const */ var BUBBLE_PADDING = 12;
    $('bubble').showTextForElement($('show-apps-button'),
                                   message,
                                   cr.ui.Bubble.Attachment.TOP,
                                   BUBBLE_OFFSET,
                                   BUBBLE_PADDING);
  };

  return {
    AppsMenuButton: AppsMenuButton
  };
});
