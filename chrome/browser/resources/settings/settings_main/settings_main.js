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

    /** @private */
    showNoResultsFound_: {
      type: Boolean,
      value: false,
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

    // Wait for any other changes prior to calculating the overflow padding.
    this.async(function() {
      this.$.overscroll.style.paddingBottom = this.overscrollHeight_() + 'px';
    });
  },

  /**
   * Return the height that the over scroll padding should be set to.
   * This is used to determine how much padding to apply to the end of the
   * content so that the last element may align with the top of the content
   * area.
   * @return {number}
   * @private
   */
  overscrollHeight_: function() {
    if (!this.currentRoute || this.currentRoute.subpage.length != 0 ||
        this.showPages_.about) {
      return 0;
    }

    // Ensure any dom-if reflects the current properties.
    Polymer.dom.flush();

    /**
     * @param {!Element} element
     * @return {number}
     */
    var calcHeight = function(element) {
      var style = getComputedStyle(element);
      var height = this.parentNode.scrollHeight - element.offsetHeight +
          parseFloat(style.marginTop) + parseFloat(style.marginBottom);
      assert(height >= 0);
      return height;
    }.bind(this);

    if (this.showPages_.advanced) {
      var lastSection = this.$$('settings-advanced-page').$$(
          'settings-section:last-of-type');
      // |lastSection| may be null in unit tests.
      if (!lastSection)
        return 0;
      return calcHeight(lastSection);
    }

    assert(this.showPages_.basic);
    var lastSection = this.$$('settings-basic-page').$$(
        'settings-section:last-of-type');
    // |lastSection| may be null in unit tests.
    if (!lastSection)
      return 0;
    return calcHeight(lastSection) - this.$$('#toggleContainer').offsetHeight;
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
    this.toolbarSpinnerActive = true;

    // Trigger rendering of the basic and advanced pages and search once ready.
    this.showPages_ = {about: false, basic: true, advanced: true};

    setTimeout(function() {
      settings.getSearchManager().search(
          query, assert(this.$$('settings-basic-page')));
    }.bind(this), 0);
    setTimeout(function() {
      settings.getSearchManager().search(
          query, assert(this.$$('settings-advanced-page'))).then(
          function(request) {
            if (!request.finished) {
              // Nothing to do here. A previous search request was canceled
              // because a new search request was issued before the first one
              // completed.
              return;
            }

            this.toolbarSpinnerActive = false;
            this.showNoResultsFound_ =
                !request.isSame('') && !request.didFindMatches();
          }.bind(this));
    }.bind(this), 0);
  },
});
