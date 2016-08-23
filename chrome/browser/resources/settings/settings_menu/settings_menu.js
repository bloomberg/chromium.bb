// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-menu' shows a menu with a hardcoded set of pages and subpages.
 */
Polymer({
  is: 'settings-menu',

  behaviors: [settings.RouteObserverBehavior],

  properties: {
    /** @private */
    aboutSelected_: Boolean,

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
              settings.Route.ADVANCED.contains(settings.getCurrentRoute()));
  },

  /**
   * @param {!settings.Route} newRoute
   */
  currentRouteChanged: function(newRoute) {
    // Make the three menus mutually exclusive.
    if (settings.Route.ABOUT.contains(newRoute)) {
      this.aboutSelected_ = true;
      this.$.advancedMenu.selected = null;
      this.$.basicMenu.selected = null;
    } else if (settings.Route.ADVANCED.contains(newRoute)) {
      this.aboutSelected_ = false;
      // For routes from URL entry, we need to set selected.
      this.$.advancedMenu.selected = newRoute.path;
      this.$.basicMenu.selected = null;
    } else if (settings.Route.BASIC.contains(newRoute)) {
      this.aboutSelected_ = false;
      this.$.advancedMenu.selected = null;
      // For routes from URL entry, we need to set selected.
      this.$.basicMenu.selected = newRoute.path;
    }
  },

  /**
   * @param {!Event} event
   * @private
   */
  ripple_: function(event) {
    var ripple = document.createElement('paper-ripple');
    ripple.addEventListener('transitionend', function() {
      ripple.remove();
    });
    event.currentTarget.appendChild(ripple);
    ripple.downAction();
    ripple.upAction();
  },

  /**
   * @param {!Event} event
   * @private
   */
  openPage_: function(event) {
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
