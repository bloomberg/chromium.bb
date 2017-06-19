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
     * Whether the external container will accept change-route-source-click
     * events.
     * @private {boolean}
     */
    changeRouteSourceAvailable_: {
      type: Boolean,
      computed: 'computeChangeRouteSourceAvailable_(route, sink,' +
          'isAnySinkCurrentlyLaunching, shownCastModeValue)',
    },

    /**
     * An enum value to represent the controller to show.
     * @private {number}
     */
    controllerType_: {
      type: Number,
      computed: 'computeControllerType_(route, isExtensionViewReady)',
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
     * Whether the custom controller extensionview is ready to be shown.
     * @type {boolean}
     */
    isExtensionViewReady: {
      type: Boolean,
      value: false,
    },

    /**
     * The timestamp for when the route details view was opened. We initialize
     * the value in a function so that the value is set when the element is
     * loaded, rather than at page load.
     * @private {number}
     */
    openTime_: {
      type: Number,
      value: function() {
        return Date.now();
      },
    },

    /**
     * The route to show.
     * @type {?media_router.Route|undefined}
     */
    route: {
      type: Object,
      observer: 'onRouteChange_',
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
   * @param {boolean} changeRouteSourceAvailable
   * @return {boolean} Whether to show the button that allows casting to the
   *     current route or the current route's sink.
   */
  computeCastButtonHidden_: function(route, changeRouteSourceAvailable) {
    return !((route && route.canJoin) || changeRouteSourceAvailable);
  },

  /**
   * @param {?media_router.Route|undefined} route The current route for the
   *     route details view.
   * @param {?media_router.Sink|undefined} sink Sink associated with |route|.
   * @param {boolean} isAnySinkCurrentlyLaunching Whether a sink is launching
   *     now.
   * @param {number} shownCastModeValue Currently selected cast mode value or
   *     AUTO if no value has been explicitly selected.
   * @return {boolean} Whether the change route source function should be
   *     available when displaying |currentRoute| in the route details view.
   *     Changing the route source should not be available when the currently
   *     selected source that would be cast is the same as the route's current
   *     source.
   * @private
   */
  computeChangeRouteSourceAvailable_: function(
      route, sink, isAnySinkCurrentlyLaunching, shownCastModeValue) {
    if (isAnySinkCurrentlyLaunching || !route || !sink) {
      return false;
    }
    if (!route.currentCastMode) {
      return true;
    }
    var selectedCastMode =
        this.computeSelectedCastMode_(shownCastModeValue, sink);
    return (selectedCastMode != 0) &&
        (selectedCastMode != route.currentCastMode);
  },

  /**
   * @param {?media_router.Route} route
   * @param {boolean} isExtensionViewReady
   * @return {number} An enum value to represent the controller to show.
   * @private
   */
  computeControllerType_: function(route, isExtensionViewReady) {
    if (route && route.supportsWebUiController) {
      return media_router.ControllerType.WEBUI;
    }
    if (isExtensionViewReady) {
      return media_router.ControllerType.EXTENSION;
    }
    return media_router.ControllerType.NONE;
  },

  /**
   * @param {number} castMode User selected cast mode or AUTO.
   * @param {?media_router.Sink} sink Sink to which we will cast.
   * @return {number} The selected cast mode when |castMode| is selected in the
   *     dialog and casting to |sink|.  Returning 0 means there is no cast mode
   *     available to |sink| and therefore the start-casting-to-route button
   *     will not be shown.
   */
  computeSelectedCastMode_: function(castMode, sink) {
    // |sink| can be null when there is a local route, which is shown in the
    // dialog, but the sink to which it is connected isn't in the current set of
    // sinks known to the dialog.  This can happen, for example, with DIAL
    // devices.  A route is created to a DIAL device, but opening the dialog on
    // a tab that only supports mirroring will not show the DIAL device.  The
    // route will be shown in route details if it is the only local route, so
    // you arrive at this function with a null |sink|.
    if (!sink) {
      return 0;
    }
    if (castMode == media_router.CastModeType.AUTO) {
      return sink.castModes & -sink.castModes;
    }
    return castMode & sink.castModes;
  },

  /**
   * Updates |activityStatus_| for the default view.
   *
   * @private
   */
  updateActivityStatus_: function() {
    this.activityStatus_ = this.route ?
        loadTimeData.getStringF(
            'castingActivityStatus', this.route.description) :
        '';
  },

  /**
   * Called when the route details view is closed. Resets route-controls.
   */
  onClosed: function() {
    if (this.controllerType_ === media_router.ControllerType.WEBUI &&
        this.$$('route-controls')) {
      this.$$('route-controls').reset();
    }
  },

  /**
   * Called when the route details view is opened.
   */
  onOpened: function() {
    if (this.controllerType_ === media_router.ControllerType.WEBUI &&
        this.$$('route-controls')) {
      media_router.ui.setRouteControls(
          /** @type {RouteControlsInterface} */ (this.$$('route-controls')));
    }
  },

  /**
   * Updates either the extensionview or the WebUI route controller, depending
   * on which should be shown.
   * @private
   */
  onRouteChange_: function(newRoute) {
    if (this.controllerType_ !== media_router.ControllerType.WEBUI) {
      this.updateActivityStatus_();
    }
    if (newRoute &&
        this.controllerType_ === media_router.ControllerType.WEBUI &&
        this.$$('route-controls')) {
      this.$$('route-controls').onRouteUpdated(newRoute);
    }
  },

  /**
   * @param {?media_router.Route} route
   * @return {boolean}
   * @private
   */
  shouldAttemptLoadingExtensionView_: function(route) {
    return !!route && !route.supportsWebUiController;
  },

  /**
   * @param {number} controllerType
   * @return {boolean} Whether the extensionview should be shown instead of the
   *     default route info element or the WebUI route controller.
   * @private
   */
  shouldShowExtensionView_: function(controllerType) {
    return controllerType === media_router.ControllerType.EXTENSION;
  },

  /**
   * @param {number} controllerType
   * @return {boolean} Whether the route info element should be shown instead of
   *     the extensionview or the WebUI route controller.
   * @private
   */
  shouldShowRouteInfoOnly_: function(controllerType) {
    return controllerType === media_router.ControllerType.NONE;
  },

  /**
   * @param {number} controllerType
   * @return {boolean} Whether the WebUI route controller should be shown
   *     instead of the default route info element or the extensionview.
   * @private
   */
  shouldShowWebUiControls_: function(controllerType) {
    return controllerType === media_router.ControllerType.WEBUI;
  },

  /**
   * Fires a join-route-click event if the current route is joinable, otherwise
   * it fires a change-route-source-click event, which changes the source of the
   * current route. This may cause the current route to be closed and a new
   * route to be started. This is called when the button to start casting to the
   * current route is clicked.
   *
   * @private
   */
  startCastingToRoute_: function() {
    if (this.route.canJoin) {
      this.fire('join-route-click', {route: this.route});
    } else {
      this.fire('change-route-source-click', {
        route: this.route,
        selectedCastMode:
            this.computeSelectedCastMode_(this.shownCastModeValue, this.sink)
      });
    }
  },
});
