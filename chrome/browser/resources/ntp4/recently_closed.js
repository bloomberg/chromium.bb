// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The recently closed menu: button, model data, and menu.
 */

cr.define('ntp4', function() {
  'use strict';

  var localStrings = new LocalStrings();

  /**
   * Returns the text used for a recently closed window.
   * @param {number} numTabs Number of tabs in the window.
   * @return {string} The text to use.
   */
  function formatTabsText(numTabs) {
    if (numTabs == 1)
      return localStrings.getString('closedwindowsingle');
    return localStrings.getStringF('closedwindowmultiple', numTabs);
  }

  var Menu = cr.ui.Menu;
  var MenuItem = cr.ui.MenuItem;
  var MenuButton = cr.ui.MenuButton;
  var RecentMenuButton = cr.ui.define('button');

  RecentMenuButton.prototype = {
    __proto__: MenuButton.prototype,

    decorate: function() {
      MenuButton.prototype.decorate.call(this);
      this.menu = new Menu;
      cr.ui.decorate(this.menu, Menu);
      this.menu.classList.add('recent-menu');
      document.body.appendChild(this.menu);

      this.needsRebuild_ = true;
      this.hidden = true;
    },

    /**
     * Shows the menu, first rebuilding it if necessary.
     * TODO(estade): the right of the menu should align with the right of the
     * button.
     * @override
     */
    showMenu: function() {
      if (this.needsRebuild_) {
        this.menu.textContent = '';
        this.dataItems_.forEach(this.addItem_, this);
        this.needsRebuild_ = false;
      }

      MenuButton.prototype.showMenu.call(this);
    },

    /**
     * Sets the menu model data.
     * @param {Array} dataItems Array of objects that describe the apps.
     */
    set dataItems(dataItems) {
      this.dataItems_ = dataItems;
      this.needsRebuild_ = true;
      this.hidden = dataItems.length == 0;
    },

    /**
     * Adds an app to the menu.
     * @param {Object} data An object encapsulating all data about the app.
     * @private
     */
    addItem_: function(data) {
      var isWindow = data.type == 'window';
      var a = this.ownerDocument.createElement('a');
      a.className = 'recent-menu-item';
      if (isWindow) {
        a.href = '';
        a.classList.add('recent-window');
        a.textContent = formatTabsText(data.tabs.length);
      } else {
        a.href = data.url;
        a.style.backgroundImage = 'url(chrome://favicon/' + data.url + ')';
        a.textContent = data.title;
        // TODO(estade): add app ping url.
      }

      function onActivate(e) {
        // TODO(estade): don't convert to string.
        chrome.send('reopenTab', [String(data.sessionId)]);
        e.preventDefault();
      }
      a.addEventListener('activate', onActivate);

      this.menu.appendChild(a);
      cr.ui.decorate(a, MenuItem);
    },
  };

  return {
    RecentMenuButton: RecentMenuButton,
  };
});
