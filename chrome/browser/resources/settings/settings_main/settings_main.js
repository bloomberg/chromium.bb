// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{about: boolean, basic: boolean, advanced: boolean}}
 */
var MainPageVisibility;

/**
 * @fileoverview
 * 'settings-main' displays the selected settings page.
 */
Polymer({
  is: 'settings-main',

  behaviors: [settings.RouteObserverBehavior],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private */
    advancedToggleExpanded_: {
      type: Boolean,
      value: false,
    },

    /**
     * True if a section is fully expanded to hide other sections beneath it.
     * Not true otherwise (even while animating a section open/closed).
     * @private
     */
    hasExpandedSection_: Boolean,

    /** @private */
    overscroll_: {
      type: Number,
      observer: 'overscrollChanged_',
    },

    /**
     * Controls which main pages are displayed via dom-ifs.
     * @private {!MainPageVisibility}
     */
    showPages_: {
      type: Object,
      value: function() {
        return {about: false, basic: false, advanced: false};
      },
    },

    /**
     * The main pages that were displayed before search was initiated. When
     * |null| it indicates that currently the page is displaying its normal
     * contents, instead of displaying search results.
     * @private {?MainPageVisibility}
     */
    previousShowPages_: {
      type: Object,
      value: null,
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

    /**
     * Dictionary defining page visibility.
     * @type {!GuestModePageVisibility}
     */
    pageVisibility: {
      type: Object,
      value: function() { return {}; },
    },
  },

  /** @override */
  attached: function() {
    document.addEventListener('toggle-advanced-page', function(e) {
      this.advancedToggleExpanded_ = e.detail;
      this.currentRouteChanged(settings.getCurrentRoute());
    }.bind(this));

    var currentRoute = settings.getCurrentRoute();
    this.hasExpandedSection_ = currentRoute && currentRoute.isSubpage();
  },

  /** @private */
  overscrollChanged_: function() {
    if (!this.overscroll_ && this.boundScroll_) {
      this.offsetParent.removeEventListener('scroll', this.boundScroll_);
      this.boundScroll_ = null;
    } else if (this.overscroll_ && !this.boundScroll_) {
      this.boundScroll_ = this.setOverscroll_.bind(this, 0);
      this.offsetParent.addEventListener('scroll', this.boundScroll_);
    }
  },

  /**
   * Sets the overscroll padding. Never forces a scroll, i.e., always leaves
   * any currently visible overflow as-is.
   * @param {number=} opt_minHeight The minimum overscroll height needed.
   */
  setOverscroll_: function(opt_minHeight) {
    var scroller = this.offsetParent;
    if (!scroller)
      return;
    var overscroll = this.$.overscroll;
    var visibleBottom = scroller.scrollTop + scroller.clientHeight;
    var overscrollBottom = overscroll.offsetTop + overscroll.scrollHeight;
    // How much of the overscroll is visible (may be negative).
    var visibleOverscroll = overscroll.scrollHeight -
                            (overscrollBottom - visibleBottom);
    this.overscroll_ = Math.max(opt_minHeight || 0, visibleOverscroll);
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
   * @return {boolean}
   * @private
   */
  showAdvancedToggle_: function() {
    var inSearchMode = !!this.previousShowPages_;
    return !inSearchMode && this.showPages_.basic && !this.hasExpandedSection_;
  },

  currentRouteChanged: function(newRoute) {
    // When the route changes from a sub-page to the main page, immediately
    // update hasExpandedSection_ to unhide the other sections.
    if (!newRoute.isSubpage())
      this.hasExpandedSection_ = false;

    this.updatePagesShown_();
  },

  /** @private */
  onSubpageExpand_: function() {
    // The subpage finished expanding fully. Hide pages other than the current
    // section's parent page.
    this.hasExpandedSection_ = true;
    this.updatePagesShown_();
  },

  /**
   * Updates the hidden state of the about, basic and advanced pages, based on
   * the current route and the Advanced toggle state.
   * @private
   */
  updatePagesShown_: function() {
    var currentRoute = settings.getCurrentRoute();
    if (settings.Route.ABOUT.contains(currentRoute)) {
      this.showPages_ = {about: true, basic: false, advanced: false};
    } else {
      this.showPages_ = {
        about: false,
        basic: settings.Route.BASIC.contains(currentRoute) ||
            !this.hasExpandedSection_,
        advanced: settings.Route.ADVANCED.contains(currentRoute) ||
            (!this.hasExpandedSection_ && this.advancedToggleExpanded_),
      };

      if (this.showPages_.advanced) {
        assert(!this.pageVisibility ||
            this.pageVisibility.advancedSettings !== false);
        this.advancedToggleExpanded_ = true;
      }
    }

    // Wait for any other changes prior to calculating the overflow padding.
    setTimeout(function() {
      // Ensure any dom-if reflects the current properties.
      Polymer.dom.flush();

      this.setOverscroll_(this.overscrollHeight_());
    }.bind(this));
  },

  /**
   * Return the height that the overscroll padding should be set to.
   * This is used to determine how much padding to apply to the end of the
   * content so that the last element may align with the top of the content
   * area.
   * @return {number}
   * @private
   */
  overscrollHeight_: function() {
    var route = settings.getCurrentRoute();
    if (!route.section || route.isSubpage() || this.showPages_.about)
      return 0;

    var page = this.getPage_(route);
    var section = page && page.getSection(route.section);
    if (!section || !section.offsetParent)
      return 0;

    // Find the distance from the section's top to the overscroll.
    var sectionTop = section.offsetParent.offsetTop + section.offsetTop;
    var distance = this.$.overscroll.offsetTop - sectionTop;

    return Math.max(0, this.offsetParent.clientHeight - distance);
  },

  /** @private */
  toggleAdvancedPage_: function() {
    this.fire('toggle-advanced-page', !this.advancedToggleExpanded_);
  },

  /**
   * Returns the root page (if it exists) for a route.
   * @param {!settings.Route} route
   * @return {(?SettingsAboutPageElement|?SettingsAdvancedPageElement|
   *           ?SettingsBasicPageElement)}
   */
  getPage_: function(route) {
    if (settings.Route.ABOUT.contains(route)) {
      return /** @type {?SettingsAboutPageElement} */(
          this.$$('settings-about-page'));
    }
    if (settings.Route.ADVANCED.contains(route)) {
      return /** @type {?SettingsAdvancedPageElement} */(
          this.$$('settings-advanced-page'));
    }
    if (settings.Route.BASIC.contains(route)) {
      return /** @type {?SettingsBasicPageElement} */(
          this.$$('settings-basic-page'));
    }
    assertNotReached();
  },

  /**
   * Navigates to the default search page (if necessary).
   * @private
   */
  ensureInDefaultSearchPage_: function() {
    if (settings.getCurrentRoute() != settings.Route.BASIC)
      settings.navigateTo(settings.Route.BASIC);
  },

  /**
   * @param {string} query
   * @return {!Promise} A promise indicating that searching finished.
   */
  searchContents: function(query) {
    if (!this.previousShowPages_) {
      // Store which pages are shown before search, so that they can be restored
      // after the user clears the search results.
      this.previousShowPages_ = this.showPages_;
    }

    this.ensureInDefaultSearchPage_();
    this.toolbarSpinnerActive = true;

    // Trigger rendering of the basic and advanced pages and search once ready.
    this.showPages_ = {about: false, basic: true, advanced: true};

    return new Promise(function(resolve, reject) {
      setTimeout(function() {
        var whenSearchDone = settings.getSearchManager().search(
            query, assert(this.getPage_(settings.Route.BASIC)));
        assert(
            whenSearchDone ===
                settings.getSearchManager().search(
                    query, assert(this.getPage_(settings.Route.ADVANCED))));

        whenSearchDone.then(function(request) {
          resolve();
          if (!request.finished) {
            // Nothing to do here. A previous search request was canceled
            // because a new search request was issued before the first one
            // completed.
            return;
          }

          this.toolbarSpinnerActive = false;
          var showingSearchResults = !request.isSame('');
          this.showNoResultsFound_ =
              showingSearchResults && !request.didFindMatches();

          if (!showingSearchResults) {
            // Restore the pages that were shown before search was initiated.
            this.showPages_ = assert(this.previousShowPages_);
            this.previousShowPages_ = null;
          }
        }.bind(this));
      }.bind(this), 0);
    }.bind(this));
  },

  /**
   * @param {(boolean|undefined)} visibility
   * @return {boolean} True unless visibility is false.
   * @private
   */
  showAdvancedSettings_: function(visibility) {
    return visibility !== false;
  },
});
