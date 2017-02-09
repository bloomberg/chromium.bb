// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{about: boolean, settings: boolean}}
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

    advancedToggleExpanded: {
      type: Boolean,
      notify: true,
    },

    /** @private */
    overscroll_: {
      type: Number,
      observer: 'overscrollChanged_',
    },

    /**
     * Controls which main pages are displayed via dom-ifs, based on the current
     * route.
     * @private {!MainPageVisibility}
     */
    showPages_: {
      type: Object,
      value: function() {
        return {about: false, settings: false};
      },
    },

    /**
     * Whether a search operation is in progress or previous search results are
     * being displayed.
     * @private {boolean}
     */
    inSearchMode_: {
      type: Boolean,
      value: false,
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

    showAndroidApps: Boolean,
  },

  /** @override */
  attached: function() {
    this.listen(this, 'freeze-scroll', 'onFreezeScroll_');
  },

  /** @override */
  detached: function() {
    this.unlisten(this, 'freeze-scroll', 'onFreezeScroll_');
  },

  /** @private */
  overscrollChanged_: function() {
    if (!this.overscroll_ && this.boundScroll_) {
      this.offsetParent.removeEventListener('scroll', this.boundScroll_);
      window.removeEventListener('resize', this.boundScroll_);
      this.boundScroll_ = null;
    } else if (this.overscroll_ && !this.boundScroll_) {
      this.boundScroll_ = function() {
        if (!this.ignoreScroll_)
          this.setOverscroll_(0);
      }.bind(this);
      this.offsetParent.addEventListener('scroll', this.boundScroll_);
      window.addEventListener('resize', this.boundScroll_);
    }
  },

  /**
   * Sets the overscroll padding. Never forces a scroll, i.e., always leaves
   * any currently visible overflow as-is.
   * @param {number=} opt_minHeight The minimum overscroll height needed.
   * @private
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
    this.overscroll_ =
        Math.max(opt_minHeight || 0, Math.ceil(visibleOverscroll));
  },

  /**
   * Enables or disables user scrolling, via overscroll: hidden. Room for the
   * hidden scrollbar is added to prevent the page width from changing back and
   * forth. Also freezes the overscroll height.
   * @param {!Event} e |e.detail| is true to freeze, false to unfreeze.
   * @private
   */
  onFreezeScroll_: function(e) {
    if (e.detail) {
      // Update the overscroll and ignore scroll events.
      this.setOverscroll_(this.overscrollHeight_());
      this.ignoreScroll_ = true;

      // Prevent scrolling the container.
      var scrollerWidth = this.offsetParent.clientWidth;
      this.offsetParent.style.overflow = 'hidden';
      var scrollbarWidth = this.offsetParent.clientWidth - scrollerWidth;
      this.offsetParent.style.width = 'calc(100% - ' + scrollbarWidth + 'px)';
    } else {
      this.ignoreScroll_ = false;
      this.offsetParent.style.overflow = '';
      this.offsetParent.style.width = '';
    }
  },

  /** @param {!settings.Route} newRoute */
  currentRouteChanged: function(newRoute) {
    this.updatePagesShown_();
  },

  /** @private */
  onSubpageExpand_: function() {
    this.updatePagesShown_();
  },

  /**
   * Updates the hidden state of the about and settings pages based on the
   * current route.
   * @private
   */
  updatePagesShown_: function() {
    var inAbout = settings.Route.ABOUT.contains(settings.getCurrentRoute());
    this.showPages_ = {about: inAbout, settings: !inAbout};

    // Calculate and set the overflow padding.
    this.updateOverscrollForPage_();

    // Wait for any other changes, then calculate the overflow padding again.
    setTimeout(function() {
      // Ensure any dom-if reflects the current properties.
      Polymer.dom.flush();
      this.updateOverscrollForPage_();
    }.bind(this));
  },

  /**
   * Calculates the necessary overscroll and sets the overscroll to that value
   * (at minimum). For the About page, this just zeroes the overscroll.
   * @private
   */
  updateOverscrollForPage_: function() {
    if (this.showPages_.about) {
      // Set overscroll directly to remove any existing overscroll that
      // setOverscroll_ would otherwise preserve.
      this.overscroll_ = 0;
      return;
    }
    this.setOverscroll_(this.overscrollHeight_());
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

  /**
   * Returns the root page (if it exists) for a route.
   * @param {!settings.Route} route
   * @return {(?SettingsAboutPageElement|?SettingsBasicPageElement)}
   */
  getPage_: function(route) {
    if (settings.Route.ABOUT.contains(route)) {
      return /** @type {?SettingsAboutPageElement} */(
          this.$$('settings-about-page'));
    }
    if (settings.Route.BASIC.contains(route) ||
        settings.Route.ADVANCED.contains(route)) {
      return /** @type {?SettingsBasicPageElement} */(
          this.$$('settings-basic-page'));
    }
    assertNotReached();
  },

  /**
   * @param {string} query
   * @return {!Promise} A promise indicating that searching finished.
   */
  searchContents: function(query) {
    // Trigger rendering of the basic and advanced pages and search once ready.
    this.inSearchMode_ = true;
    this.toolbarSpinnerActive = true;

    return new Promise(function(resolve, reject) {
      setTimeout(function() {
        var whenSearchDone =
            assert(this.getPage_(settings.Route.BASIC)).searchContents(query);
        whenSearchDone.then(function(request) {
          resolve();
          if (!request.finished) {
            // Nothing to do here. A previous search request was canceled
            // because a new search request was issued before the first one
            // completed.
            return;
          }

          this.toolbarSpinnerActive = false;
          this.inSearchMode_ = !request.isSame('');
          this.showNoResultsFound_ =
              this.inSearchMode_ && !request.didFindMatches();
        }.bind(this));
      }.bind(this), 0);
    }.bind(this));
  },
});
