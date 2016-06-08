// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-main' displays the selected settings page.
 */
Polymer({
  is: 'settings-main',

  properties: {
    /** @private */
    isAdvancedMenuOpen_: {
      type: Boolean,
      value: false,
    },

    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * The current active route.
     * @type {!SettingsRoute}
     */
    currentRoute: {
      type: Object,
      notify: true,
      observer: 'currentRouteChanged_',
    },

    /** @private */
    showAdvancedPage_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    showAdvancedToggle_: {
      type: Boolean,
      value: true,
    },

    /** @private */
    showBasicPage_: {
      type: Boolean,
      value: true,
    },

    /** @private */
    showAboutPage_: {
      type: Boolean,
      value: false,
    },
  },

  attached: function() {
    document.addEventListener('toggle-advanced-page', function(e) {
      this.showAdvancedPage_ = e.detail;
      this.isAdvancedMenuOpen_ = e.detail;
    }.bind(this));
  },

  /**
   * @param {!SettingsRoute} newRoute
   * @private
   */
  currentRouteChanged_: function(newRoute) {
    var isSubpage = !!newRoute.subpage.length;

    this.showAboutPage_ = newRoute.page == 'about';

    this.showAdvancedToggle_ = !this.showAboutPage_ && !isSubpage;

    this.showBasicPage_ = this.showAdvancedToggle_ || newRoute.page == 'basic';

    this.showAdvancedPage_ =
        (this.isAdvancedMenuOpen_ && this.showAdvancedToggle_) ||
        newRoute.page == 'advanced';

    this.$.pageContainer.style.height = isSubpage ? '100%' : '';
  },

  /** @private */
  toggleAdvancedPage_: function() {
    this.fire('toggle-advanced-page', !this.isAdvancedMenuOpen_);
  },
});
