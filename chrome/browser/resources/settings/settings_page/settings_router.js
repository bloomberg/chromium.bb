// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-router' is a simple router for settings. Its responsibilities:
 *  - Update the URL when the routing state changes.
 *  - Initialize the routing state with the initial URL.
 *  - Process and validate all routing state changes.
 *
 * Example:
 *
 *    <settings-router current-route="{{currentRoute}}">
 *    </settings-router>
 */
Polymer({
  is: 'settings-router',

  properties: {
    /**
     * The current active route. This may only be updated via the global
     * function settings.navigateTo.
     *
     * currentRoute.page refers to top-level pages such as Basic and Advanced.
     *
     * currentRoute.section is only non-empty when the user is on a subpage. If
     * the user is on Basic, for instance, this is an empty string.
     *
     * currentRoute.subpage is an Array. The last element is the actual subpage
     * the user is on. The previous elements are the ancestor subpages. This
     * enables support for multiple paths to the same subpage. This is used by
     * both the Back button and the Breadcrumb to determine ancestor subpages.
     * @type {!settings.Route}
     */
    currentRoute: {
      notify: true,
      type: Object,
      value: function() {
        return this.getRouteFor_(window.location.pathname);
      },
    },
  },

  /**
   * Sets up a history popstate observer.
   * @override
   */
  created: function() {
    window.addEventListener('popstate', function(event) {
      // On pop state, do not push the state onto the window.history again.
      this.currentRoute = this.getRouteFor_(window.location.pathname);
    }.bind(this));

    settings.navigateTo = this.navigateTo_.bind(this);
  },

  /**
   * Returns the matching canonical route, or the default route if none matches.
   * @param {string} path
   * @return {!settings.Route}
   * @private
   */
  getRouteFor_: function(path) {
    // TODO(tommycli): Use Object.values once Closure compilation supports it.
    var matchingKey = Object.keys(settings.Route).find(function(key) {
      return settings.Route[key].path == path;
    });

    if (!matchingKey)
      return settings.Route.BASIC;

    return settings.Route[matchingKey];
  },

  /**
   * Navigates to a canonical route.
   * @param {!settings.Route} route
   * @private
   */
  navigateTo_: function(route) {
    assert(!!route);

    if (route == this.currentRoute)
      return;

    window.history.pushState(undefined, document.title, route.path);
    this.currentRoute = route;
  },
});
