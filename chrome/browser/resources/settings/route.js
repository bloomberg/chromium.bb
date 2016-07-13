// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /**
   * Class for navigable routes. May only be instantiated within this file.
   * @constructor
   * @param {string} url
   * @private
   */
  var Route = function(url) {
    this.url = url;

    /** @private {?settings.Route} */
    this.parent_ = null;

    // Below are all legacy properties to provide compatibility with the old
    // routing system. TODO(tommycli): Remove once routing refactor complete.
    this.page = '';
    this.section = '';
    /** @type {!Array<string>} */ this.subpage = [];
    this.dialog = false;
  };

  Route.prototype = {
    /**
     * Returns a new Route instance that's a child of this route.
     * @param {string} url
     * @param {string=} opt_subpageName
     * @return {!settings.Route}
     * @private
     */
    createChild: function(url, opt_subpageName) {
      var route = new Route(url);
      route.parent_ = this;
      route.page = this.page;
      route.section = this.section;
      route.subpage = this.subpage.slice();  // Shallow copy.

      if (opt_subpageName)
        route.subpage.push(opt_subpageName);

      return route;
    },

    /**
     * Returns a new Route instance that's a child dialog of this route.
     * @param {string} url
     * @return {!settings.Route}
     * @private
     */
    createDialog: function(url) {
      var route = this.createChild(url);
      route.dialog = true;
      return route;
    },

    /**
     * Returns true if this route is a descendant of the parameter.
     * @param {!settings.Route} route
     * @return {boolean}
     */
    isDescendantOf: function(route) {
      for (var parent = this.parent_; parent != null; parent = parent.parent_) {
        if (route == parent)
          return true;
      }

      return false;
    },
  };

  return {
    Route: Route,
  };
});
