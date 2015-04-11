// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

    Polymer('more-route', {
      publish: {
        /**
         * The name of this route. Behavior differs based on the presence of
         * `path` during _declaration_.
         *
         * If `path` is present during declaration, it is registered via `name`.
         *
         * Otherwise, this `more-route` becomes a `reference` to the route with
         * `name`. Changing `name` will update which route is referenced.
         *
         * @attribute name
         * @type String
         */
        name: '',

        /**
         * A path expression used to parse parameters from the window's URL.
         *
         * @attribute path
         * @type String
         */
        path: '',

        /**
         * Whether this route should become a context for the element that
         * contains it.
         *
         * @attribute context
         * @type Boolean
         * @default false
         */
        context: false,

        /**
         * The full path expression for this route, including routes this is
         * nested within.
         *
         * @attribute fullPath
         * @type String
         * @readonly
         */
        fullPath: '',

        /**
         * Whether this route is a reference to a named route.
         *
         * @attribute reference
         * @type Boolean
         */
        reference: false,

        /**
         * Param values matching the current URL, or an empty object if not
         * `active`.
         *
         * @attribute params
         * @type Object
         */
        params: {},

        /**
         * Whether the route matches the current URL.
         *
         * @attribute active
         * @type Boolean
         * @readonly
         */
        active: false,

        /**
         * The underlying `MoreRouting.Route` object that is being wrapped.
         *
         * @attribute route
         * @type MoreRouting.Route
         * @readonly
         */
        route: null,

        /**
         * The parent `more-route` element, if it is nested within another.
         *
         * Parents are found by searching up this element's ancestry for nodes
         * that are `more-route` elements, or have a `moreRouteContext`
         * property.
         *
         * @attribute parent
         * @type <more-route>
         * @readonly
         */
        parent: null,
      },

      observe: {
        'name':      '_identityChanged',
        'path':      '_identityChanged',
        'reference': '_identityChanged',
        // TODO(nevir): Truly read-only properties would be super handy.
        'active':    '_updateProxiedValues',
      },

      /** @override */
      routingReady: function() {
        this.reference = this.name && !this.path;
        this._identityChangedCallCount = 0;
        this._identityChanged();
      },

      urlFor: function(params) {
        return this.route.urlFor(params);
      },

      navigateTo: function(params) {
        return this.route.navigateTo(params);
      },

      isCurrentUrl: function(params) {
        return this.route.isCurrentUrl(params);
      },

      _identityChanged: function() {
        if (!this.routingIsReady) return;

        this._identityChangedCallCount = this._identityChangedCallCount + 1;
        if (this._identityChangedCallCount === 2) return;

        if (this.name && this.reference) {
          this.route = MoreRouting.getRouteByName(this.name);
        } else if (this.path && this.name) {
          this.route = MoreRouting.registerNamedRoute(this.name, this.path, this.parentRoute);
        } else if (this.path) {
          this.route = MoreRouting.getRouteByPath(this.path, this.parentRoute);
        } else {
          this.route = null;
        }

        if (this._routeObserver) this._routeObserver.close();
        if (this.route) {
          this.params   = this.route.params;
          this.path     = this.route.path;
          this.fullPath = this.route.fullPath;
          this._observeRoute(this.route);
        } else {
          this.params   = {};
          this.path     = '';
          this.fullPath = '';
        }

        if (this.hasAttribute('context')) {
          var parent = this.parentElement || this.parentNode && this.parentNode.host;
          if (!parent) {
            console.warn(this, 'wants to create a routing context, but it has no parent');
            return;
          }
          parent.moreRouteContext = this.route;
        }
      },

      _updateProxiedValues: function() {
        var previousActive = this.active;
        this.active = this.route && this.route.active;

        if (this.active === previousActive) return;
        var eventName = 'more-route-' + (this.active ? 'active' : 'inactive');
        this.fire(eventName, this.route);
      },

      _observeRoute: function(route) {
        this._routeObserver = new CompoundObserver();
        this._routeObserver.addPath(route, 'active');
        this._routeObserver.open(this._updateProxiedValues.bind(this));
      },

    });
  
