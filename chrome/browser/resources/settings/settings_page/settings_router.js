// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-router' is a simple router for settings. Its responsibilites:
 *  - Update the URL when the routing state changes.
 *  - Initialize the routing state with the initial URL.
 *  - Process and validate all routing state changes.
 *
 * Example:
 *
 *    <settings-router current-route="{{currentRoute}}">
 *    </settings-router>
 *
 * @group Chrome Settings Elements
 * @element settings-router
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
     */
    currentRoute: {
      type: Object,
      value: function() {
        // Take the current URL, find a matching pre-defined route, and
        // initialize the currentRoute to that pre-defined route.
        for (var i = 0; i < this.routes_.length; ++i) {
          var route = this.routes_[i];
          if (route.url == window.location.pathname) {
            return {
              page: route.page,
              section: route.section,
              subpage: route.subpage,
            };
          }
        }

        // As a fallback return the default route.
        return this.routes_[0];
      },
      notify: true,
      observer: 'currentRouteChanged_',
    },
  },


 /**
  * @private
  * The 'url' property is not accessible to other elements.
  */
 routes_: [
    {
      url: '/',
      page: 'basic',
      section: '',
      subpage: [],
    },
    {
      url: '/advanced',
      page: 'advanced',
      section: '',
      subpage: [],
    },
    {
      url: '/searchEngines',
      page: 'basic',
      section: 'search',
      subpage: ['search-engines'],
    },
    {
      url: '/searchEngines/advanced',
      page: 'basic',
      section: 'search',
      subpage: ['search-engines', 'search-engines-advanced'],
    },
  ],

  /**
   * @private
   * Is called when another element modifies the route. This observer validates
   * the route change against the pre-defined list of routes, and updates the
   * URL appropriately.
   */
  currentRouteChanged_: function() {
    for (var i = 0; i < this.routes_.length; ++i) {
      var route = this.routes_[i];
      if (route.page == this.currentRoute.page &&
          route.section == this.currentRoute.section &&
          route.subpage.length == this.currentRoute.subpage.length &&
          this.currentRoute.subpage.every(function(value, index) {
            return value == route.subpage[index];
          })) {
        history.pushState(null, null, route.url);
        return;
      }
    }

    assertNotReached('Route not found: ' + JSON.stringify(this.currentRoute));
  },
});
