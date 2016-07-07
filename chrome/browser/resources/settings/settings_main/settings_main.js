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

  /** @override */
  created: function() {
    /** @private {!PromiseResolver} */
    this.resolver_ = new PromiseResolver;
    settings.main.rendered = this.resolver_.promise;
  },

  attached: function() {
    document.addEventListener('toggle-advanced-page', function(e) {
      this.showAdvancedPage_ = e.detail;
      this.isAdvancedMenuOpen_ = e.detail;
      this.currentRoute = {
        page: this.isAdvancedMenuOpen_ ? 'advanced' : 'basic',
        section: '',
        subpage: [],
      };
      if (this.showAdvancedPage_) {
        doWhenReady(
            function() {
              var advancedPage = this.$$('settings-advanced-page');
              return !!advancedPage && advancedPage.scrollHeight > 0;
            }.bind(this),
            function() {
              this.$$('#toggleContainer').scrollIntoView();
            }.bind(this));
      }
    }.bind(this));

    doWhenReady(
        function() {
          var basicPage = this.$$('settings-basic-page');
          return !!basicPage && basicPage.scrollHeight > 0;
        }.bind(this),
        function() {
          this.resolver_.resolve();
        }.bind(this));
  },

  /**
   * @param {boolean} opened Whether the menu is expanded.
   * @return {string} Which icon to use.
   * @private
   * */
  arrowState_: function(opened) {
    return opened ? 'settings:arrow-drop-up' : 'cr:arrow-drop-down';
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

    this.style.height = isSubpage ? '100%' : '';
  },

  /** @private */
  toggleAdvancedPage_: function() {
    this.fire('toggle-advanced-page', !this.isAdvancedMenuOpen_);
  },

  /**
   * Navigates to the default search page (if necessary).
   * @private
   */
  ensureInDefaultSearchPage_: function() {
    if (this.currentRoute.page != 'basic' ||
        this.currentRoute.section != '' ||
        this.currentRoute.subpage.length != 0) {
      this.currentRoute = {page: 'basic', section: '', subpage: [], url: ''};
    }
  },

  /**
   * @param {string} query
   */
  searchContents: function(query) {
    this.ensureInDefaultSearchPage_();

    // Trigger rendering of the basic and advanced pages and search once ready.
    // Even if those are already rendered, yield to the message loop before
    // initiating searching.
    this.showBasicPage_ = true;
    setTimeout(function() {
      settings.search(query, assert(this.$$('settings-basic-page')));
    }.bind(this), 0);

    this.showAdvancedPage_ = true;
    setTimeout(function() {
      settings.search(query, assert(this.$$('settings-advanced-page')));
    }.bind(this), 0);
  },
});
