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

    /** @private */
    inSubpage_: Boolean,

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
      settings.navigateTo(this.advancedToggleExpanded_ ?
          settings.Route.ADVANCED : settings.Route.BASIC);
    }.bind(this));

  },

  /** @private */
  overscrollChanged_: function() {
    if (!this.overscroll_ && this.boundScroll_) {
      this.parentNode.scroller.removeEventListener('scroll', this.boundScroll_);
      this.boundScroll_ = null;
    } else if (this.overscroll_ && !this.boundScroll_) {
      this.boundScroll_ = this.scrollEventListener_.bind(this);
      this.parentNode.scroller.addEventListener('scroll', this.boundScroll_);
    }
  },

  /** @private */
  scrollEventListener_: function() {
    var scroller = this.parentNode.scroller;
    var overscroll = this.$.overscroll;
    var visibleBottom = scroller.scrollTop + scroller.clientHeight;
    var overscrollBottom = overscroll.offsetTop + overscroll.scrollHeight;
    // How much of the overscroll is visible (may be negative).
    var visibleOverscroll = overscroll.scrollHeight -
                            (overscrollBottom - visibleBottom);
    this.overscroll_ = Math.max(0, visibleOverscroll);
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
    return this.showPages_.basic && !this.inSubpage_ && !inSearchMode;
  },

  /** @protected */
  currentRouteChanged: function(newRoute) {
    this.inSubpage_ = newRoute.isSubpage();
    this.style.height = this.inSubpage_ ? '100%' : '';

    if (settings.Route.ABOUT.contains(newRoute)) {
      this.showPages_ = {about: true, basic: false, advanced: false};
    } else {
      this.showPages_ = {
        about: false,
        basic: settings.Route.BASIC.contains(newRoute) || !this.inSubpage_,
        advanced: settings.Route.ADVANCED.contains(newRoute) ||
            (!this.inSubpage_ && this.advancedToggleExpanded_),
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

      this.overscroll_ = this.overscrollHeight_();
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
    if (route.isSubpage() || this.showPages_.about)
      return 0;

    var query = 'settings-section[section="' + route.section + '"]';
    var topSection = this.$$('settings-basic-page').$$(query);
    if (!topSection && this.showPages_.advanced)
      topSection = this.$$('settings-advanced-page').$$(query);

    if (!topSection || !topSection.offsetParent)
      return 0;

    // Offset to the selected section (relative to the scrolling window).
    let sectionTop = topSection.offsetParent.offsetTop + topSection.offsetTop;
    // The height of the selected section and remaining content (sections).
    let heightOfShownSections = this.$.overscroll.offsetTop - sectionTop;
    return Math.max(0, this.parentNode.scrollHeight - heightOfShownSections);
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
            query, assert(this.$$('settings-basic-page')));
        assert(
            whenSearchDone ===
                settings.getSearchManager().search(
                    query, assert(this.$$('settings-advanced-page'))));

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
