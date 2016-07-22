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
    advancedToggleExpanded_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    inSubpage_: Boolean,

    /**
     * Controls which main pages are displayed via dom-ifs.
     * @type {!{about: boolean, basic: boolean, advanced: boolean}}
     * @private
     */
    showPages_: {
      type: Object,
      value: function() {
        return {about: false, basic: false, advanced: false};
      },
    },

    toolbarSpinnerActive: {
      type: Boolean,
      value: false,
      notify: true,
    },
  },

  /** @override */
  created: function() {
    /** @private {!PromiseResolver} */
    this.resolver_ = new PromiseResolver;
    settings.main.rendered = this.resolver_.promise;
  },

  /** @override */
  ready: function() {
    settings.getSearchManager().setCallback(function(isRunning) {
      this.toolbarSpinnerActive = isRunning;
    }.bind(this));
  },

  /** @override */
  attached: function() {
    document.addEventListener('toggle-advanced-page', function(e) {
      this.advancedToggleExpanded_ = e.detail;
      this.currentRoute = {
        page: this.advancedToggleExpanded_ ? 'advanced' : 'basic',
        section: '',
        subpage: [],
      };
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
   */
  arrowState_: function(opened) {
    return opened ? 'settings:arrow-drop-up' : 'cr:arrow-drop-down';
  },

  /**
   * @param {boolean} showBasicPage
   * @param {boolean} inSubpage
   * @return {boolean}
   */
  showAdvancedToggle_: function(showBasicPage, inSubpage) {
    return showBasicPage && !inSubpage;
  },

  /**
   * @param {!SettingsRoute} newRoute
   * @private
   */
  currentRouteChanged_: function(newRoute) {
    this.inSubpage_ = newRoute.subpage.length > 0;
    this.style.height = this.inSubpage_ ? '100%' : '';

    if (newRoute.page == 'about') {
      this.showPages_ = {about: true, basic: false, advanced: false};
    } else {
      this.showPages_ = {
        about: false,
        basic: newRoute.page == 'basic' || !this.inSubpage_,
        advanced: newRoute.page == 'advanced' ||
            (!this.inSubpage_ && this.advancedToggleExpanded_),
      };

      if (this.showPages_.advanced)
        this.advancedToggleExpanded_ = true;
    }
  },

  /** @private */
  toggleAdvancedPage_: function() {
    this.fire('toggle-advanced-page', !this.advancedToggleExpanded_);
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
    this.showPages_ = {about: false, basic: true, advanced: true};
    setTimeout(function() {
      settings.getSearchManager().search(
          query, assert(this.$$('settings-basic-page')));
    }.bind(this), 0);
    setTimeout(function() {
      settings.getSearchManager().search(
          query, assert(this.$$('settings-advanced-page')));
    }.bind(this), 0);
  },
});
