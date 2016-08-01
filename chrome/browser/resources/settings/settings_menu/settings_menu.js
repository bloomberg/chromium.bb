// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-menu' shows a menu with a hardcoded set of pages and subpages.
 */
Polymer({
  is: 'settings-menu',

  properties: {
    /** @private */
    advancedOpened_: Boolean,

    /**
     * The current active route.
     * @type {!settings.Route}
     */
    currentRoute: {
      type: Object,
      notify: true,
    },

    /**
     * Dictionary defining page visibility.
     * @type {!GuestModePageVisibility}
     */
    pageVisibility: {
      type: Object,
    },
  },

  attached: function() {
    document.addEventListener('toggle-advanced-page', function(e) {
      if (e.detail)
        this.$.advancedPage.open();
      else
        this.$.advancedPage.close();
    }.bind(this));

    this.$.advancedPage.addEventListener('paper-submenu-open', function() {
      this.fire('toggle-advanced-page', true);
    }.bind(this));

    this.$.advancedPage.addEventListener('paper-submenu-close', function() {
      this.fire('toggle-advanced-page', false);
    }.bind(this));

    this.fire('toggle-advanced-page',
              settings.Route.ADVANCED.contains(this.currentRoute));
  },

  /**
   * @param {!Node} target
   * @private
   */
  ripple_: function(target) {
    var ripple = document.createElement('paper-ripple');
    ripple.addEventListener('transitionend', function() {
      ripple.remove();
    });
    target.appendChild(ripple);
    ripple.downAction();
    ripple.upAction();
  },

  /**
   * @param {!Event} event
   * @private
   */
  openPage_: function(event) {
    this.ripple_(/** @type {!Node} */(event.currentTarget));

    var route = settings.getRouteForPath(event.currentTarget.dataset.path);
    assert(route, 'settings-menu has an an entry with an invalid path');
    settings.navigateTo(route);
  },

  /**
   * @param {boolean} opened Whether the menu is expanded.
   * @return {string} Which icon to use.
   * @private
   * */
  arrowState_: function(opened) {
    return opened ? 'settings:arrow-drop-up' : 'cr:arrow-drop-down';
  },
});
