// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   dialog: (string|undefined),
 *   page: string,
 *   section: string,
 *   subpage: !Array<string>,
 * }}
 */
var SettingsRoute;

/** @typedef {SettingsRoute|{url: string}} */
var CanonicalRoute;

/** @typedef {SettingsRoute|{inHistory: boolean}} */
var HistoricRoute;

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
     * The current active route. This is reflected to the URL. Updates to this
     * property should replace the whole object.
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
     * @type {SettingsRoute}
     */
    currentRoute: {
      notify: true,
      observer: 'currentRouteChanged_',
      type: Object,
      value: function() {
        var initialRoute = this.canonicalRoutes_[0];

        // Take the current URL, find a matching pre-defined route, and
        // initialize the currentRoute to that pre-defined route.
        for (var i = 0; i < this.canonicalRoutes_.length; ++i) {
          var canonicalRoute = this.canonicalRoutes_[i];
          if (canonicalRoute.url == window.location.pathname) {
            initialRoute = canonicalRoute;
            break;
          }
        }

        return {
          page: initialRoute.page,
          section: initialRoute.section,
          subpage: initialRoute.subpage,
          dialog: initialRoute.dialog,
        };
      },
    },

    /**
     * Page titles for the currently active route. Updated by the currentRoute
     * property observer.
     * @type {{pageTitle: string}}
     */
    currentRouteTitles: {
      notify: true,
      type: Object,
      value: function() {
        return {
          pageTitle: '',
        };
      },
    },
  },


  /**
   * @private {!Array<!CanonicalRoute>}
   * The 'url' property is not accessible to other elements.
   */
  canonicalRoutes_: Object.keys(settings.Route).map(function(key) {
    return settings.Route[key];
  }),

  /**
   * Sets up a history popstate observer.
   */
  created: function() {
    window.addEventListener('popstate', function(event) {
      if (event.state && event.state.page)
        this.currentRoute = event.state;
    }.bind(this));
  },

  /**
   * Is called when another element modifies the route. This observer validates
   * the route change against the pre-defined list of routes, and updates the
   * URL appropriately.
   * @param {!SettingsRoute} newRoute Where we're headed.
   * @param {!SettingsRoute|undefined} oldRoute Where we've been.
   * @private
   */
  currentRouteChanged_: function(newRoute, oldRoute) {
    for (var i = 0; i < this.canonicalRoutes_.length; ++i) {
      var canonicalRoute = this.canonicalRoutes_[i];
      if (canonicalRoute.page == newRoute.page &&
          canonicalRoute.section == newRoute.section &&
          canonicalRoute.dialog == newRoute.dialog &&
          canonicalRoute.subpage.length == newRoute.subpage.length &&
          canonicalRoute.subpage.every(function(value, index) {
            return value == newRoute.subpage[index];
          })) {
        // Update the property containing the titles for the current route.
        this.currentRouteTitles = {
          pageTitle: loadTimeData.getString(canonicalRoute.page + 'PageTitle'),
        };

        // If we are restoring a state from history, don't push it again.
        if (/** @type {HistoricRoute} */(newRoute).inHistory)
          return;

        // Mark routes persisted in history as already stored in history.
        var historicRoute = /** @type {HistoricRoute} */({
          inHistory: true,
          page: newRoute.page,
          section: newRoute.section,
          subpage: newRoute.subpage,
          dialog: newRoute.dialog,
        });

        // Push the current route to the history state, so when the user
        // navigates with the browser back button, we can recall the route.
        if (oldRoute) {
          window.history.pushState(historicRoute, document.title,
                                   canonicalRoute.url);
        } else {
          // For the very first route (oldRoute will be undefined), we replace
          // the existing state instead of pushing a new one. This is to allow
          // the user to use the browser back button to exit Settings entirely.
          window.history.replaceState(historicRoute, document.title);
        }

        return;
      }
    }

    assertNotReached('Route not found: ' + JSON.stringify(newRoute));
  },
});
