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
     * Whether a sink is currently launching in the container.
     * @type {boolean}
     */
    isAnySinkCurrentlyLaunching: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether the external container will accept replace-route-click events.
     * @private {boolean}
     */
    replaceRouteAvailable_: {
      type: Boolean,
      computed: 'computeReplaceRouteAvailable_(route, sink,' +
                    'isAnySinkCurrentlyLaunching, shownCastModeValue)',
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
     * The cast mode shown to the user. Initially set to auto mode. (See
     * media_router.CastMode documentation for details on auto mode.)
     * @type {number}
     */
    shownCastModeValue: {
      type: Number,
      value: media_router.AUTO_CAST_MODE.type,
    },

    /**
     * Sink associated with |route|.
     * @type {?media_router.Sink}
     */
    sink: {
      type: Object,
      value: null,
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
   * @param {boolean} replaceRouteAvailable
   * @return {boolean} Whether to show the button that allows casting to the
   *     current route or the current route's sink.
   */
  computeCastButtonHidden_: function(route, replaceRouteAvailable) {
    return !((route && route.canJoin) || replaceRouteAvailable);
  },

  /**
   * @param {?media_router.Route|undefined} route The current route for the
   *     route details view.
   * @param {?media_router.Sink|undefined} sink Sink associated with |route|.
   * @param {boolean} isAnySinkCurrentlyLaunching Whether a sink is launching
   *     now.
   * @param {number} shownCastModeValue Currently selected cast mode value or
   *     AUTO if no value has been explicitly selected.
   * @return {boolean} Whether the replace route function should be available
   *     when displaying |currentRoute| in the route details view. Replace route
   *     should not be available when the source that would be cast from when
   *     creating a new route would be the same as the route's current source.
   * @private
   */
  computeReplaceRouteAvailable_: function(
      route, sink, isAnySinkCurrentlyLaunching, shownCastModeValue) {
    if (isAnySinkCurrentlyLaunching || !route || !sink) {
      return false;
    }
    if (!route.currentCastMode) {
      return true;
    }
    var selectedCastMode = shownCastModeValue;
    if (selectedCastMode == media_router.CastModeType.AUTO) {
      selectedCastMode = sink.castModes & -sink.castModes;
    }
    return ((selectedCastMode & sink.castModes) != 0) &&
        (selectedCastMode != route.currentCastMode);
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
