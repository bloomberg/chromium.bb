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
   * @param {!settings.Route} oldRoute
   */
  currentRouteChanged: function(newRoute, oldRoute) {
    var newRouteIsSubpage = newRoute && newRoute.subpage.length;
    var oldRouteIsSubpage = oldRoute && oldRoute.subpage.length;

    if (!oldRoute && newRouteIsSubpage) {
      // Allow the page to load before expanding the section. TODO(michaelpg):
      // Time this better when refactoring settings-animated-pages.
      setTimeout(function() {
        var section = this.getSection_(newRoute.section);
        if (section)
          this.expandSection_(section);
      }.bind(this));
      return;
    }

    if (newRouteIsSubpage) {
      if (!oldRouteIsSubpage || newRoute.section != oldRoute.section) {
        var section = this.getSection_(newRoute.section);
        if (section)
          this.expandSection_(section);
      }
    } else {
      if (oldRouteIsSubpage) {
        var section = this.getSection_(oldRoute.section);
        if (section)
          this.collapseSection_(section);
      }

      // Scrolls to the section if this main page contains the route's section.
      if (newRoute && newRoute.section && this.getSection_(newRoute.section))
        this.scrollToSection_();
    }
  },

  /**
   * Animates the card in |section|, expanding it to fill the page.
   * @param {!SettingsSectionElement} section
   * @private
   */
  expandSection_: function(section) {
    // If another section's card is expanding, cancel that animation first.
    var expanding = this.$$('.expanding');
    if (expanding) {
      if (expanding == section)
        return;

      if (this.animations['section']) {
        // Cancel the animation, then call startExpandSection_.
        this.cancelAnimation('section', function() {
          this.startExpandSection_(section);
        }.bind(this));
      } else {
        // The animation must have finished but its promise hasn't resolved yet.
        // When it resolves, collapse that section's card before expanding
        // this one.
        setTimeout(function() {
          this.collapseSection_(
              /** @type {!SettingsSectionElement} */(expanding));
          this.finishAnimation('section', function() {
            this.startExpandSection_(section);
          }.bind(this));
        }.bind(this));
      }

      return;
    }

    if (this.$$('.collapsing') && this.animations['section']) {
      // Finish the collapse animation before expanding.
      this.finishAnimation('section', function() {
        this.startExpandSection_(section);
      }.bind(this));
      return;
    }

    this.startExpandSection_(section);
  },

  /**
   * Helper function to set up and start the expand animation.
   * @param {!SettingsSectionElement} section
   */
  startExpandSection_: function(section) {
    if (!section.canAnimateExpand())
      return;

    // Freeze the scroller and save its position.
    this.listScrollTop_ = this.scroller.scrollTop;

    var scrollerWidth = this.scroller.clientWidth;
    this.scroller.style.overflow = 'hidden';
    // Adjust width to compensate for scroller.
    var scrollbarWidth = this.scroller.clientWidth - scrollerWidth;
    this.scroller.style.width = 'calc(100% - ' + scrollbarWidth + 'px)';

    // Freezes the section's height so its card can be removed from the flow.
    section.setFrozen(true);

    // Expand the section's card to fill the parent.
    var animationPromise = this.playExpandSection_(section);

    animationPromise.then(function() {
      this.scroller.scrollTop = 0;
      this.toggleOtherSectionsHidden_(section.section, true);
    }.bind(this), function() {
      // Animation was canceled; restore the section.
      section.setFrozen(false);
    }.bind(this)).then(function() {
      this.scroller.style.overflow = '';
      this.scroller.style.width = '';
    }.bind(this));
  },

  /**
   * Expands the card in |section| to fill the page.
   * @param {!SettingsSectionElement} section
   * @return {!Promise}
   * @private
   */
  playExpandSection_: function(section) {
    // We must be attached.
    assert(this.scroller);

    var promise;
    var animationConfig = section.animateExpand(this.scroller);
    if (animationConfig) {
      promise = this.animateElement('section', animationConfig.card,
          animationConfig.keyframes, animationConfig.options);
    } else {
      promise = Promise.resolve();
    }

    var finished;
    promise.then(function() {
      finished = true;
      this.style.margin = 'auto';
    }.bind(this), function() {
      // The animation was canceled; catch the error and continue.
      finished = false;
    }).then(function() {
      section.cleanUpAnimateExpand(finished);
    });

    return promise;
  },

  /**
   * Animates the card in |section|, collapsing it back into its section.
   * @param {!SettingsSectionElement} section
   * @private
   */
  collapseSection_: function(section) {
    // If the section's card is still expanding, cancel the expand animation.
    if (section.classList.contains('expanding')) {
      if (this.animations['section']) {
        this.cancelAnimation('section');
      } else {
        // The animation must have finished but its promise hasn't finished
        // resolving; try again asynchronously.
        this.async(function() {
          this.collapseSection_(section);
        });
      }
      return;
    }

    if (!section.canAnimateCollapse())
      return;

    this.toggleOtherSectionsHidden_(section.section, false);

    var scrollerWidth = this.scroller.clientWidth;
    this.scroller.style.overflow = 'hidden';
    // Adjust width to compensate for scroller.
    var scrollbarWidth = this.scroller.clientWidth - scrollerWidth;
    this.scroller.style.width = 'calc(100% - ' + scrollbarWidth + 'px)';

    this.playCollapseSection_(section).then(function() {
      section.setFrozen(false);
      this.scroller.style.overflow = '';
      this.scroller.style.width = '';
      section.classList.remove('collapsing');
    }.bind(this));
  },

  /**
   * Collapses the card in |section| back to its normal position.
   * @param {!SettingsSectionElement} section
   * @return {!Promise}
   * @private
   */
  playCollapseSection_: function(section) {
    // We must be attached.
    assert(this.scroller);

    this.style.margin = '';

    var promise;
    var animationConfig =
        section.animateCollapse(this.scroller, this.listScrollTop_);
    if (animationConfig) {
      promise = this.animateElement('section', animationConfig.card,
          animationConfig.keyframes, animationConfig.options);
    } else {
      promise = Promise.resolve();
    }

    promise.then(function() {
      section.cleanUpAnimateCollapse(true);
    }, function() {
      section.cleanUpAnimateCollapse(false);
    });
    return promise;
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
          this.getSection_(settings.getCurrentRoute().section).scrollIntoView();
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

  /**
   * Helper function to get a section from the local DOM.
   * @param {string} section Section name of the element to get.
   * @return {?SettingsSectionElement}
   * @private
   */
  getSection_: function(section) {
    return /** @type {?SettingsSectionElement} */(
        this.$$('[section=' + section + ']'));
  },
};

/** @polymerBehavior */
var MainPageBehavior = [
  settings.RouteObserverBehavior,
  TransitionBehavior,
  MainPageBehaviorImpl,
];
