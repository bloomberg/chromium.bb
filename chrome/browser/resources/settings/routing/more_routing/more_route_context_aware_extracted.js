// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

    Polymer('more-route-context-aware', {

      /** @override */
      ready: function() {
        // We're being stamped by a template; we need to defer until the
        // template instance has been appended to the document.
        if (!this.parentElement && !this.host && this.templateInstance) return;
        this._routeContextAwareReady();
      },

      /** @override */
      domReady: function() {
        this._routeContextAwareReady();
      },

      /**
       * Extension point for subclasses. They can assume that the element has
       * been initialized by this point.
       *
       * If your element is to become a route context, it must define that route
       * **synchronously** within this callback!
       */
      routingReady: function() {},

      _routeContextAwareReady: function() {
        if (this.routingIsReady) return;
        this.routingIsReady = true;
        this._awakenRouteContextAwareAncestors();
        this.parentRoute = this._findParentRoute();
        this.routingReady();
      },

      /**
       * Annoyingly, `ready` doesn't appear to fire in a consistent manner
       * between the elements that extend this. We want top-down loading of our
       * elements so that parents can be properly built and resolved.
       */
      _awakenRouteContextAwareAncestors: function() {
        var ancestors = this._findRouteContextAwareAncestors();
        for (var i = 0, ancestor; ancestor = ancestors[i]; i++) {
          ancestor._routeContextAwareReady();
        }
      },

      _findParentRoute: function() {
        var node = this;
        while (node) {
          node = node.parentNode || node.host;
          var route = node && (node.moreRouteContext || node.route);
          if (route instanceof MoreRouting.Route) {
            return route;
          }
        }
        return null;
      },

      _findRouteContextAwareAncestors: function() {
        var node = this;
        var ancestors = [];
        while (node) {
          node = node.parentNode || node.host;
          if (node && node._routeContextAwareReady) {
            ancestors.push(node);
          }
        }
        return ancestors;
      }

    });
  
