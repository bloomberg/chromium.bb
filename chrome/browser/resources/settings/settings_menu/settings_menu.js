// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-menu' shows a menu with a hardcoded set of pages and subpages.
 *
 * Example:
 *
 *     <settings-menu selected-page-id="{{selectedPageId}}">
 *     </settings-menu>
 */
Polymer({
  is: 'settings-menu',

  properties: {
    /**
     * The current active route.
     */
    currentRoute: {
      type: Object,
      notify: true,
      observer: 'currentRouteChanged_',
    },
  },

  /** @private */
  currentRouteChanged_: function() {
    // Sync URL changes to the side nav menu.

    this.$.advancedPage.opened = this.currentRoute.page == 'advanced';
    this.$.basicPage.opened = this.currentRoute.page == 'basic';

    if (this.$.advancedPage.opened)
      this.$.advancedMenu.selected = this.currentRoute.section;

    if (this.$.basicPage.opened)
      this.$.basicMenu.selected = this.currentRoute.section;
  },

  /** @private */
  openPage_: function(event) {
    var submenuRoute = event.currentTarget.parentNode.dataset.page;
    if (submenuRoute) {
      this.currentRoute = {
        page: submenuRoute,
        section: event.currentTarget.dataset.section,
        subpage: [],
      };
    }
  },

  /**
   * @param {boolean} opened Whether the menu is expanded.
   * @return {string} Which icon to use.
   * @private
   * */
  arrowState_: function(opened) {
    return opened ? 'settings:arrow-drop-up' : 'settings:arrow-drop-down';
  },
});
