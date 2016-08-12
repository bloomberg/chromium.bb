// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Calls |readyTest| repeatedly until it returns true, then calls
 * |readyCallback|.
 * @param {function():boolean} readyTest
 * @param {!Function} readyCallback
 */
function doWhenReady(readyTest, readyCallback) {
  // TODO(dschuyler): Determine whether this hack can be removed.
  // See also: https://github.com/Polymer/polymer/issues/3629
  var intervalId = setInterval(function() {
    if (readyTest()) {
      clearInterval(intervalId);
      readyCallback();
    }
  }, 10);
}

/**
 * Responds to route changes by expanding, collapsing, or scrolling to sections
 * on the page. Expanded sections take up the full height of the container. At
 * most one section should be expanded at any given time.
 * @polymerBehavior MainPageBehavior
 */
var MainPageBehaviorImpl = {
  /** @type {?HTMLElement} The scrolling container. */
  scroller: null,

  /** @override */
  attached: function() {
    if (this.domHost && this.domHost.parentNode.tagName == 'PAPER-HEADER-PANEL')
      this.scroller = this.domHost.parentNode.scroller;
    else
      this.scroller = document.body; // Used in unit tests.
  },

  /**
   * @param {!settings.Route} newRoute
   * @param {settings.Route} oldRoute
   */
  currentRouteChanged: function(newRoute, oldRoute) {
    // Allow the page to load before expanding the section. TODO(michaelpg):
    // Time this better when refactoring settings-animated-pages.
    if (!oldRoute && newRoute.isSubpage())
      setTimeout(this.tryTransitionToSection_.bind(this));
    else
      this.tryTransitionToSection_();
  },

  /**
   * If possible, transitions to the current route's section (by expanding or
   * scrolling to it). If another transition is running, finishes or cancels
   * that one, then schedules this function again. This ensures the current
   * section is quickly shown, without getting the page into a broken state --
   * if currentRoute changes in between calls, just transition to the new route.
   * @private
   */
  tryTransitionToSection_: function() {
    var currentRoute = settings.getCurrentRoute();
    var currentSection = this.getSection_(currentRoute.section);

    // If an animation is already playing, try finishing or canceling it.
    if (this.currentAnimation_) {
      this.maybeStopCurrentAnimation_();
      // Either way, this function will be called again once the current
      // animation ends.
      return;
    }

    var promise;
    var expandedSection = /** @type {?SettingsSectionElement} */(
        this.$$('settings-section.expanded'));
    if (expandedSection) {
      // If the section shouldn't be expanded, collapse it.
      if (!currentRoute.isSubpage() || expandedSection != currentSection) {
        promise = this.collapseSection_(expandedSection);
        // Scroll to the collapsed section. TODO(michaelpg): This can look weird
        // because the collapse we just scheduled calculated its end target
        // based on the current scroll position. This bug existed before, and is
        // fixed in the next patch by making the card position: absolute.
        if (currentSection)
          this.scrollToSection_();
      }
    } else if (currentSection) {
      // Expand the section into a subpage or scroll to it on the main page.
      if (currentRoute.isSubpage())
        promise = this.expandSection_(currentSection);
      else
        this.scrollToSection_();
    }

    // When this animation ends, another may be necessary. Call this function
    // again after the promise resolves.
    if (promise)
      promise.then(this.tryTransitionToSection_.bind(this));
  },

  /**
   * If the current animation is inconsistent with the current route, stops the
   * animation by finishing or canceling it so the new route can be animated to.
   * @private
   */
  maybeStopCurrentAnimation_: function() {
    var currentRoute = settings.getCurrentRoute();
    var animatingSection = /** @type {?SettingsSectionElement} */(
        this.$$('settings-section.expanding, settings-section.collapsing'));
    assert(animatingSection);

    if (animatingSection.classList.contains('expanding')) {
      // Cancel the animation to go back to the main page if the animating
      // section shouldn't be expanded.
      if (animatingSection.section != currentRoute.section ||
          !currentRoute.isSubpage()) {
        this.currentAnimation_.cancel();
      }
      // Otherwise, let the expand animation continue.
      return;
    }

    assert(animatingSection.classList.contains('collapsing'));
    if (!currentRoute.isSubpage())
      return;

    // If the collapsing section actually matches the current route's section,
    // we can just cancel the animation to re-expand the section.
    if (animatingSection.section == currentRoute.section) {
      this.currentAnimation_.cancel();
      return;
    }

    // The current route is a subpage, so that section needs to expand.
    // Immediately finish the current collapse animation so that can happen.
    this.currentAnimation_.finish();
  },

  /**
   * Animates the card in |section|, expanding it to fill the page.
   * @param {!SettingsSectionElement} section
   * @return {!Promise} Resolved when the transition is finished or canceled.
   * @private
   */
  expandSection_: function(section) {
    assert(this.scroller);
    assert(section.canAnimateExpand());

    // Save the scroller position before freezing it.
    this.origScrollTop_ = this.scroller.scrollTop;
    this.toggleScrolling_(false);

    // Freeze the section's height so its card can be removed from the flow.
    section.setFrozen(true);

    this.currentAnimation_ = section.animateExpand(this.scroller);
    var promise = this.currentAnimation_ ?
        this.currentAnimation_.finished : Promise.resolve();

    var finished;
    return promise.then(function() {
      this.scroller.scrollTop = 0;
      this.toggleOtherSectionsHidden_(section.section, true);
      finished = true;
    }.bind(this), function() {
      // The animation was canceled; restore the section.
      section.setFrozen(false);
      finished = false;
    }).then(function() {
      section.cleanUpAnimateExpand(finished);
      this.toggleScrolling_(true);
      this.currentAnimation_ = null;
    }.bind(this));
  },

  /**
   * Animates the card in |section|, collapsing it back into its section.
   * @param {!SettingsSectionElement} section
   * @return {!Promise} Resolved when the transition is finished or canceled.
   * @private
   */
  collapseSection_: function(section) {
    assert(this.scroller);
    assert(section.canAnimateCollapse());

    this.toggleOtherSectionsHidden_(section.section, false);
    this.toggleScrolling_(false);

    this.currentAnimation_ =
        section.animateCollapse(this.scroller, this.origScrollTop_);
    var promise = this.currentAnimation_ ?
        this.currentAnimation_.finished : Promise.resolve();

    return promise.then(function() {
      section.cleanUpAnimateCollapse(true);
    }, function() {
      section.cleanUpAnimateCollapse(false);
    }).then(function() {
      section.setFrozen(false);
      section.classList.remove('collapsing');
      this.toggleScrolling_(true);
      this.currentAnimation_ = null;
    }.bind(this));
  },

  /**
   * Hides or unhides the sections not being expanded.
   * @param {string} sectionName The section to keep visible.
   * @param {boolean} hidden Whether the sections should be hidden.
   * @private
   */
  toggleOtherSectionsHidden_: function(sectionName, hidden) {
    var sections = Polymer.dom(this.root).querySelectorAll(
        'settings-section');
    for (var section of sections)
      section.hidden = hidden && (section.section != sectionName);
  },

  /** @private */
  scrollToSection_: function() {
    doWhenReady(
        function() {
          return this.scrollHeight > 0;
        }.bind(this),
        function() {
          // If the current section changes while we are waiting for the page to
          // be ready, scroll to the newest requested section.
          var section = this.getSection_(settings.getCurrentRoute().section);
          if (section)
            section.scrollIntoView();
        }.bind(this));
  },

  /**
   * Helper function to get a section from the local DOM.
   * @param {string} section Section name of the element to get.
   * @return {?SettingsSectionElement}
   * @private
   */
  getSection_: function(section) {
    if (!section)
      return null;
    return /** @type {?SettingsSectionElement} */(
        this.$$('[section=' + section + ']'));
  },

  /**
   * Enables or disables user scrolling, via overscroll: hidden. Room for the
   * hidden scrollbar is added to prevent the page width from changing back and
   * forth.
   * @param {boolean} enabled
   * @private
   */
  toggleScrolling_: function(enabled) {
    if (enabled) {
      this.scroller.style.overflow = '';
      this.scroller.style.width = '';
    } else {
      var scrollerWidth = this.scroller.clientWidth;
      this.scroller.style.overflow = 'hidden';
      var scrollbarWidth = this.scroller.clientWidth - scrollerWidth;
      this.scroller.style.width = 'calc(100% - ' + scrollbarWidth + 'px)';
    }
  }
};

/** @polymerBehavior */
var MainPageBehavior = [
  settings.RouteObserverBehavior,
  MainPageBehaviorImpl,
];
