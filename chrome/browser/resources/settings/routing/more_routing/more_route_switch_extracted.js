// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

    Polymer('more-route-switch', {
      publish: {
        routeContext: false,
        activeRoute: null,
        paramsProperty: 'params',
      },

      ready: function() {
        if (this.routeContext) {
          this.moreRouteContext = this.$.activeRouteEl;
        }
      },

      attached: function() {
        // We need to defer attaching until the DOM has fully resolved, so that
        // any parent routes are discovered.
        this.async(function() {
          this.observeChildren();
          this.evaluate();
        });
      },

      /** @override */
      observeChildren: function() {
        this._observer = new CompoundObserver();
        for (var i = 0, child; child = this.children[i]; i++) {
          this._observer.addPath(this._routeForChild(child), 'active');
        }
        this._observer.open(this.evaluate.bind(this));
      },

      /** @override */
      childIsTruthy: function(child, index) {
        return this._routeForChild(child).active;
      },

      /** @override */
      activateChild: function(child, index, external) {
        this.super(arguments);
        if (!child) {
          this.activeRoute = null;
          return;
        }
        this.activeRoute = this._routeForChild(child);
        this.$.activeRouteEl.path = this.activeRoute.path;

        if (!external) return;
        this.activeRoute.navigateTo(this._paramsForChild(child));
      },

      /** @override */
      modelForChild: function(child, index) {
        var model = Object.create(this.super(arguments));
        model[this.paramsProperty] = this._paramsForChild(child);
        return model;
      },

      _paramsForChild: function(child) {
        var route = this._routeForChild(child);
        return route && route.active ? route.params : this._parentParams;
      },

      get _parentParams() {
        return this.$.activeRouteEl.parent && this.$.activeRouteEl.parent.params || {};
      },

      _routeForChild: function(child) {
        var pathOrName = child.getAttribute('when-route') || '/';
        if (!MoreRouting.isPath(pathOrName)) {
          return MoreRouting.getRouteByName(pathOrName);
        }

        var path = pathOrName;
        if (this.$.activeRouteEl.parent) {
          path = MoreRouting.joinPath(this.$.activeRouteEl.parent.path, path);
        }
        return MoreRouting.getRouteByPath(path);
      },
    });
  
