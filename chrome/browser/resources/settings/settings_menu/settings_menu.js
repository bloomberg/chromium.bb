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
      observer: 'currentRouteChanged_',
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

    this.fire('toggle-advanced-page', this.currentRoute.page == 'advanced');
  },

  /**
   * @param {!settings.Route} newRoute
   * @private
   */
  currentRouteChanged_: function(newRoute) {
    // Sync URL changes to the side nav menu.

    if (newRoute.page == 'advanced') {
      assert(!this.pageVisibility ||
             this.pageVisibility.advancedSettings !== false);
      this.$.advancedMenu.selected = this.currentRoute.section;
      this.$.basicMenu.selected = null;
    } else if (newRoute.page == 'basic') {
      this.$.advancedMenu.selected = null;
      this.$.basicMenu.selected = this.currentRoute.section;
    } else {
      this.$.advancedMenu.selected = null;
      this.$.basicMenu.selected = null;
    }
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
    var page = event.currentTarget.parentNode.dataset.page;
    if (!page)
      return;

    var section = event.currentTarget.dataset.section;
    // TODO(tommycli): Use Object.values once Closure compilation supports it.
    var matchingKey = Object.keys(settings.Route).find(function(key) {
      var route = settings.Route[key];
      // TODO(tommycli): Remove usage of page, section, and subpage properties.
      // Routes should be somehow directly bound to the menu elements.
      return route.page == page && route.section == section &&
          route.subpage.length == 0;
    });

    if (matchingKey)
      settings.navigateTo(settings.Route[matchingKey]);
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
