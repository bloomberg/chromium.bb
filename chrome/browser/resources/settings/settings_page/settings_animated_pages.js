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
 *    <settings-animated-pages section="privacy">
 *      <!-- Insert your section controls here -->
 *    </settings-animated-pages>
 */

Polymer({
  is: 'settings-animated-pages',

  behaviors: [settings.RouteObserverBehavior],

  properties: {
    /**
     * Routes with this section activate this element. For instance, if this
     * property is 'search', and currentRoute.section is also set to 'search',
     * this element will display the subpage in currentRoute.subpage.
     *
     * The section name must match the name specified in route.js.
     */
    section: String,

    /**
     * A Map specifying which element should be focused when exiting a subpage.
     * The key of the map holds a settings.Route path, and the value holds
     * either a query selector that identifies the desired element, an element
     * or a function to be run when a neon-animation-finish event is handled.
     * @type {?Map<string, (string|Element|Function)>}
     */
    focusConfig: Object,
  },

  listeners: {
    'neon-animation-finish': 'onNeonAnimationFinish_',
  },

  /**
   * The last "previous" route reported by the router.
   * @private {?settings.Route}
   */
  previousRoute_: null,

  /** @override */
  created: function() {
    // Observe the light DOM so we know when it's ready.
    this.lightDomObserver_ =
        Polymer.dom(this).observeNodes(this.lightDomChanged_.bind(this));
  },

  /** @private */
  onNeonAnimationFinish_: function() {
    if (settings.lastRouteChangeWasPopstate())
      return;

    // Set initial focus when navigating to a subpage for a11y.
    let subPage = /** @type {SettingsSubpageElement} */ (
        this.querySelector('settings-subpage.iron-selected'));
    if (subPage)
      subPage.initialFocus();
  },

  /**
   * @param {!Event} e
   * @private
   */
  onIronSelect_: function(e) {
    if (!this.focusConfig || !this.previousRoute_)
      return;

    // Don't attempt to focus any anchor element, unless last navigation was a
    // 'pop' (backwards) navigation.
    if (!settings.lastRouteChangeWasPopstate())
      return;

    const subpagePaths = [];
    if (settings.routes.SITE_SETTINGS_COOKIES)
      subpagePaths.push(settings.routes.SITE_SETTINGS_COOKIES.path);

    if (settings.routes.SITE_SETTINGS_SITE_DATA)
      subpagePaths.push(settings.routes.SITE_SETTINGS_SITE_DATA.path);

    if (settings.routes.SITE_SETTINGS_ALL)
      subpagePaths.push(settings.routes.SITE_SETTINGS_ALL.path);

    // <if expr="chromeos">
    if (settings.routes.INTERNET_NETWORKS)
      subpagePaths.push(settings.routes.INTERNET_NETWORKS.path);
    // </if>

    // Only handle iron-select events from neon-animatable elements and the
    // given whitelist of settings-subpage instances.
    const whitelist = ['settings-subpage#site-settings', 'neon-animatable'];
    whitelist.push.apply(
        whitelist,
        subpagePaths.map(path => `settings-subpage[route-path="${path}"]`));
    const query = whitelist.join(', ');

    if (!e.detail.item.matches(query))
      return;

    let pathConfig = this.focusConfig.get(this.previousRoute_.path);
    if (pathConfig) {
      let handler;
      if (typeof pathConfig == 'function') {
        handler = pathConfig;
      } else {
        handler = () => {
          if (typeof pathConfig == 'string')
            pathConfig = assert(this.querySelector(pathConfig));
          cr.ui.focusWithoutInk(/** @type {!Element} */ (pathConfig));
        };
      }
      // neon-animatable has "display: none" until the animation finishes,
      // so calling focus() on any of its children has no effect until
      // "display:none" is removed. Therefore, don't set focus from within
      // the currentRouteChanged callback.
      listenOnce(this, 'neon-animation-finish', handler);
    }
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
   * Calls currentRouteChanged with the deferred route change info.
   * @private
   */
  runQueuedRouteChange_: function() {
    if (!this.queuedRouteChange_)
      return;
    this.async(this.currentRouteChanged.bind(
        this, this.queuedRouteChange_.newRoute,
        this.queuedRouteChange_.oldRoute));
  },

  /** @protected */
  currentRouteChanged: function(newRoute, oldRoute) {
    this.previousRoute_ = oldRoute;

    if (newRoute.section == this.section && newRoute.isSubpage()) {
      this.switchToSubpage_(newRoute, oldRoute);
    } else {
      this.$.animatedPages.exitAnimation = 'settings-fade-out-animation';
      this.$.animatedPages.entryAnimation = 'settings-fade-in-animation';
      this.$.animatedPages.selected = 'default';
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
      if (oldRoute.isSubpage() && newRoute.depth > oldRoute.depth) {
        const isRtl = loadTimeData.getString('textdirection') == 'rtl';
        const exit = isRtl ? 'right' : 'left';
        const entry = isRtl ? 'left' : 'right';
        this.$.animatedPages.exitAnimation = 'slide-' + exit + '-animation';
        this.$.animatedPages.entryAnimation =
            'slide-from-' + entry + '-animation';
      } else if (oldRoute.depth > newRoute.depth) {
        const isRtl = loadTimeData.getString('textdirection') == 'rtl';
        const exit = isRtl ? 'left' : 'right';
        const entry = isRtl ? 'right' : 'left';
        this.$.animatedPages.exitAnimation = 'slide-' + exit + '-animation';
        this.$.animatedPages.entryAnimation =
            'slide-from-' + entry + '-animation';
      } else {
        // The old route is not a subpage or is at the same level, so just fade.
        this.$.animatedPages.exitAnimation = 'settings-fade-out-animation';
        this.$.animatedPages.entryAnimation = 'settings-fade-in-animation';

        if (!oldRoute.isSubpage()) {
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

    this.$.animatedPages.selected = newRoute.path;
  },

  /**
   * Ensures that the template enclosing the subpage is stamped.
   * @private
   */
  ensureSubpageInstance_: function() {
    const routePath = settings.getCurrentRoute().path;
    /* TODO(dpapad): Remove conditional logic once migration to Polymer 2 is
     * completed. */
    const domIf = this.querySelector(
        (Polymer.DomIf ? 'dom-if' : 'template') +
        `[route-path='${routePath}']`);

    // Nothing to do if the subpage isn't wrapped in a <dom-if>
    // (or <template is="dom-if" for Poylmer 1) or the template is already
    // stamped.
    if (!domIf || domIf.if)
      return;

    // Set the subpage's id for use by neon-animated-pages.
    // TODO(dpapad): Remove conditional logic once migration to Polymer 2 is
    // completed.
    const content = Polymer.DomIf ?
        Polymer.DomIf._contentForTemplate(domIf.firstElementChild) :
        /** @type {{_content: DocumentFragment}} */ (domIf)._content;
    const subpage = content.querySelector('settings-subpage');
    subpage.setAttribute('route-path', routePath);

    // Carry over the
    //  1)'no-search' attribute or
    //  2) 'noSearch' Polymer property
    // template to the stamped instance (both cases are mapped to a 'no-search'
    // attribute intentionally), such that the stamped instance will also be
    // ignored by the searching algorithm.
    //
    // In the case were no-search is dynamically calculated use the following
    // pattern:
    //
    // <template is="dom-if" route-path="/myPath"
    //     no-search="[[shouldSkipSearch_(foo, bar)">
    //   <settings-subpage
    //     no-search$="[[shouldSkipSearch_(foo, bar)">
    //     ...
    //   </settings-subpage>
    //  </template>
    //
    // Note that the dom-if should always use the property and settings-subpage
    // should always use the attribute.
    if (domIf.hasAttribute('no-search') || domIf.noSearch)
      subpage.setAttribute('no-search', '');

    // Render synchronously so neon-animated-pages can select the subpage.
    domIf.if = true;
    domIf.render();
  },
});
