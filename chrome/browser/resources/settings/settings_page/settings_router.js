// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-router' is only used to propagate route changes to bound elements.
 * All the real routing is defined within route.js.
 * TODO(tommycli): Remove once all elements migrated to RouteObserverBehavior.
 *
 * Example:
 *    <settings-router current-route="{{currentRoute}}">
 *    </settings-router>
 */
Polymer({
  is: 'settings-router',

  behaviors: [settings.RouteObserverBehavior],

  properties: {
    /**
     * Only used to propagate settings.currentRoute to all the elements bound to
     * settings-router.
     * @type {!settings.Route}
     */
    currentRoute: {
      notify: true,
      type: Object,
      value: function() { return settings.getCurrentRoute(); },
    },
  },

  /** @protected */
  currentRouteChanged: function() {
    this.currentRoute = settings.getCurrentRoute();
  },
});
