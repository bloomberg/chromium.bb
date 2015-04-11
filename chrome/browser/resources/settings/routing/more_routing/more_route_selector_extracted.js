// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

    Polymer('more-route-selector', {
      publish: {
        /**
         * The attribute to read route expressions from (on children).
         *
         * @attribute routeAttribute
         * @type String
         */
        routeAttribute: 'route',

        /**
         * The index of the selected route (relative to `routes`). -1 when there
         * is no active route.
         *
         * @attribute selectedIndex
         * @type integer
         * @readonly
         */
        selectedIndex: -1,

        /**
         * The selected `MoreRouting.Route` object, or `null`.
         *
         * @attribute selectedRoute
         * @type MoreRouting.Route
         * @readonly
         */
        selectedRoute: null,

        /**
         * The path expression of the selected route, or `null` if no route.
         *
         * @attribute selectedPath
         * @type ?string
         * @readonly
         */
        selectedPath: null,

        /**
         * The params of the selected route, or an empty object if no route.
         *
         * @attribute selectedParams
         * @type Object
         * @readonly
         */
        selectedParams: {},
      },

      /**
       * @event more-route-selected fires when a new route is selected.
       * @param {{
       *   newRoute:  MoreRouting.Route, oldRoute: MoreRouting.Route,
       *   newIndex:  number,  oldIndex:  number,
       *   newPath:   ?string, oldPath:   ?string,
       *   newParams: Object,  oldParams: Object,
       * }}
       */

      /** @override */
      domReady: function() {
        this.super();
        // Rather than performing this setup in `routingReady`, we defer until
        // domReady so that our children have a chance to set up.
        this._managedSelector = this._findChildSelector();
        // `routingReady` fires top-down; we need to let them do their thing
        // before we walk them.
        this.async(function() {
          this.$.selection.routes = this._routesFromChildren();
          this._observeManagedSelector();
        });
      },

      selectedIndexChanged: function() {
        if (!this.routingIsReady || !this._managedSelector) return;
        this._managedSelector.selected = this.selectedIndex;
      },

      /**
       * Are we managing a `core-selector`?
       */
      _findChildSelector: function() {
        if (this.children.length !== 1) return null;
        var child = this.firstElementChild;
        if ('selected' in child && 'items' in child) {  // Quack!
          return child;
        }
      },

      _observeManagedSelector: function() {
        if (!this._managedSelector) return;

        this._observer = new PathObserver(this._managedSelector, 'selectedIndex');
        this._observer.open(function(newIndex) {
          if (this.selectedIndex === this._managedSelector.selectedIndex) return;
          var route = this.$.selection.routes[newIndex];
          if (!route) return;
          route.navigateTo();
        }.bind(this));
      },

      _routesFromChildren: function() {
        // If we aren't managing a `core-selector`, just use our own children.
        var context = this._managedSelector || this;
        var routes = [];
        for (var i = 0, child; child = context.children[i]; i++) {
          routes.push(this._routeForChild(child));
        }

        return routes;
      },

      _routeForChild: function(child) {
        if (child.moreRouteContext && child.moreRouteContext instanceof MoreRouting.Route) {
          return child.moreRouteContext;
        }

        if (!child.hasAttribute(this.routeAttribute)) {
          console.warn('Child of', this, 'is missing route attribute "' + this.routeAttribute + '":', child);
          return null;
        }
        var expression = child.getAttribute(this.routeAttribute);
        var route      = MoreRouting.getRoute(expression, this.parentRoute);
        // Associate the route w/ its element while we're here.
        child.moreRouteContext = route;

        return route;
      },

    });
  
