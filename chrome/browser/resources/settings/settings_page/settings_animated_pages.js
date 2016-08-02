// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-animated-pages' is a container for a page and animated subpages.
 * It provides a set of common behaviors and animations.
 *
 * Example:
 *
 *    <settings-animated-pages current-route="{{currentRoute}}"
 *        section="privacy">
 *      <!-- Insert your section controls here -->
 *    </settings-animated-pages>
 */
Polymer({
  is: 'settings-animated-pages',

  properties: {
    /**
     * Contains the current route.
     */
    currentRoute: {
      type: Object,
      notify: true,
      observer: 'currentRouteChanged_',
    },

    /**
     * Routes with this section activate this element. For instance, if this
     * property is 'search', and currentRoute.section is also set to 'search',
     * this element will display the subpage in currentRoute.subpage.
     *
     * The section name must match the name specified in settings_router.js.
     */
    section: {
      type: String,
    },
  },

  /** @override */
  created: function() {
    // Observe the light DOM so we know when it's ready.
    this.lightDomObserver_ = Polymer.dom(this).observeNodes(
        this.lightDomChanged_.bind(this));

    this.addEventListener('subpage-back', function() {
      settings.navigateTo(this.currentRoute.parent);
    }.bind(this));
  },

  /**
   * Called initially once the effective children are ready.
   * @private
   */
  lightDomChanged_: function() {
    if (this.lightDomReady_)
      return;

    this.lightDomReady_ = true;
    Polymer.dom(this).unobserveNodes(this.lightDomObserver_);
    this.runQueuedRouteChange_();
  },

  /**
   * Calls currentRouteChanged_ with the deferred route change info.
   * @private
   */
  runQueuedRouteChange_: function() {
    if (!this.queuedRouteChange_)
      return;
    this.async(this.currentRouteChanged_.bind(
        this,
        this.queuedRouteChange_.newRoute,
        this.queuedRouteChange_.oldRoute));
  },

  /** @private */
  currentRouteChanged_: function(newRoute, oldRoute) {
    if (newRoute.section == this.section && newRoute.subpage.length > 0) {
      this.switchToSubpage_(newRoute, oldRoute);
    } else {
      this.$.animatedPages.exitAnimation = 'fade-out-animation';
      this.$.animatedPages.entryAnimation = 'fade-in-animation';
      this.$.animatedPages.selected = 'main';
    }
  },

  /**
   * Selects the subpage specified by |newRoute|.
   * @param {!settings.Route} newRoute
   * @param {!settings.Route} oldRoute
   * @private
   */
  switchToSubpage_: function(newRoute, oldRoute) {
    // Don't manipulate the light DOM until it's ready.
    if (!this.lightDomReady_) {
      this.queuedRouteChange_ = this.queuedRouteChange_ || {oldRoute: oldRoute};
      this.queuedRouteChange_.newRoute = newRoute;
      return;
    }

    this.ensureSubpageInstance_();

    if (oldRoute) {
      var oldRouteIsSubpage = oldRoute.subpage.length > 0;
      if (oldRouteIsSubpage && oldRoute.contains(newRoute)) {
        // Slide left for a descendant subpage.
        this.$.animatedPages.exitAnimation = 'slide-left-animation';
        this.$.animatedPages.entryAnimation = 'slide-from-right-animation';
      } else if (newRoute.contains(oldRoute)) {
        // Slide right for an ancestor subpage.
        this.$.animatedPages.exitAnimation = 'slide-right-animation';
        this.$.animatedPages.entryAnimation = 'slide-from-left-animation';
      } else {
        // The old route is not a subpage or is at the same level, so just fade.
        this.$.animatedPages.exitAnimation = 'fade-out-animation';
        this.$.animatedPages.entryAnimation = 'fade-in-animation';

        if (!oldRouteIsSubpage) {
          // Set the height the expand animation should start at before
          // beginning the transition to the new subpage.
          // TODO(michaelpg): Remove MainPageBehavior's dependency on this
          // height being set.
          this.style.height = this.clientHeight + 'px';
          this.async(function() {
            this.style.height = '';
          });
        }
      }
    }

    this.$.animatedPages.selected = newRoute.subpage.slice(-1)[0];
  },

  /**
   * Ensures that the template enclosing the subpage is stamped.
   * @private
   */
  ensureSubpageInstance_: function() {
    var id = this.currentRoute.subpage.slice(-1)[0];
    var template = Polymer.dom(this).querySelector(
        'template[name="' + id + '"]');

    // Nothing to do if the subpage isn't wrapped in a <template> or the
    // template is already stamped.
    if (!template || template.if)
      return;

    // Set the subpage's id for use by neon-animated-pages.
    var subpage = /** @type {{_content: DocumentFragment}} */(template)._content
        .querySelector('settings-subpage');
    if (!subpage.id)
      subpage.id = id;

    // Render synchronously so neon-animated-pages can select the subpage.
    template.if = true;
    template.render();
  },
});
