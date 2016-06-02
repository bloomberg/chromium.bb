// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This Polymer element shows information from media that is currently cast
// to a device.
Polymer({
  is: 'route-details',

  properties: {
    /**
     * The text for the current casting activity status.
     * @private {string|undefined}
     */
    activityStatus_: {
      type: String,
    },

    /**
     * Bitmask of available cast modes compatible with the sink of the current
     * route.
     * @type {number}
     */
    availableCastModes: {
      type: Number,
      value: 0,
    },

    /**
     * Whether the external container will accept replace-route-click events.
     * @type {boolean}
     */
    replaceRouteAvailable: {
      type: Boolean,
      value: true,
    },

    /**
     * The route to show.
     * @type {?media_router.Route|undefined}
     */
    route: {
      type: Object,
      observer: 'maybeLoadCustomController_',
    },

    /**
     * Whether the custom controller should be hidden.
     * A custom controller is shown iff |route| specifies customControllerPath
     * and the view can be loaded.
     * @private {boolean}
     */
    isCustomControllerHidden_: {
      type: Boolean,
      value: true,
    },
  },

  behaviors: [
    I18nBehavior,
  ],

  /**
   * Fires a close-route event. This is called when the button to close
   * the current route is clicked.
   *
   * @private
   */
  closeRoute_: function() {
    this.fire('close-route', {route: this.route});
  },

  /**
   * @param {?media_router.Route|undefined} route
   * @param {number} availableCastModes
   * @param {boolean} replaceRouteAvailable
   * @return {boolean} Whether to show the button that allows casting to the
   *     current route or the current route's sink.
   */
  computeCastButtonHidden_: function(
      route, availableCastModes, replaceRouteAvailable) {
    return !((route && route.canJoin) ||
             (availableCastModes && replaceRouteAvailable));
  },

  /**
   * Fires a join-route-click event if the current route is joinable, otherwise
   * it fires a replace-route-click event, which stops the current route and
   * immediately launches a new route to the same sink. This is called when the
   * button to start casting to the current route is clicked.
   *
   * @private
   */
  startCastingToRoute_: function() {
    if (this.route.canJoin) {
      this.fire('join-route-click', {route: this.route});
    } else {
      this.fire('replace-route-click', {route: this.route});
    }
  },

  /**
   * Loads the custom controller if |route.customControllerPath| exists.
   * Falls back to the default route details view otherwise, or if load fails.
   * Updates |activityStatus_| for the default view.
   *
   * @private
   */
  maybeLoadCustomController_: function() {
    this.activityStatus_ = this.route ?
        loadTimeData.getStringF('castingActivityStatus',
                                this.route.description) :
        '';

    if (!this.route || !this.route.customControllerPath) {
      this.isCustomControllerHidden_ = true;
      return;
    }

    // Show custom controller
    var extensionview = this.$['custom-controller'];

    // Do nothing if the url is the same and the view is not hidden.
    if (this.route.customControllerPath == extensionview.src &&
        !this.isCustomControllerHidden_)
      return;

    var that = this;
    extensionview.load(this.route.customControllerPath)
    .then(function() {
      // Load was successful; show the custom controller.
      that.isCustomControllerHidden_ = false;
    }, function() {
      // Load was unsuccessful; fall back to default view.
      that.isCustomControllerHidden_ = true;
    });
  },
});
