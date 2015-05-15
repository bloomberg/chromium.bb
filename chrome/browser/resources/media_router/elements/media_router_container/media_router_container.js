// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  /**
   * The possible states of media-router-container. Used to determine which
   * components of media-router-container to show.
   *
   * @enum {number}
   */
  var MediaRouterContainerView = {
    BLOCKING_ISSUE: 0,
    ROUTE_DETAILS: 1,
    SINK_PICKER: 2,
  };

// This Polymer element contains the entire media router interface. It handles
// hiding and showing specific components.
Polymer('media-router-container', {
  publish: {
    /**
     * The list of CastModes to show.
     *
     * @attribute castModeList
     * @type {?Array<!media_router.CastMode>}
     * @default null
     */
    castModeList: null,

    /**
     * The header text.
     *
     * @attribute headerText
     * @type {?string}
     * @default null
     */
    headerText: null,

    /**
     * The issue to show.
     *
     * @attribute issue
     * @type {?media_router.Issue}
     * @default null
     */
    issue: null,

    /**
     * List of current routes.
     *
     * @attribute routeList
     * @type {?Array<!media_router.Route>}
     * @default null
     */
    routeList: null,

    /**
     * List of discovered sinks.
     *
     * @attribute sinkList
     * @type {?Array<!media_router.Sink>}
     * @default null
     */
    sinkList: null,
  },

  observe: {
    routeList: 'rebuildRouteMaps',
    selectedCastModeValue_: 'hideCastMode',
    sinkList: 'rebuildSinkMap',
  },

  created: function() {
    this.castModeList = [];
    this.routeList = [];
    this.routeMap_ = {};
    this.sinkList = [];
    this.sinkMap_ = {};
    this.sinkToRouteMap_ = {};
  },

  /**
   * Whether or not the cast-mode-picker is currently hidden.
   * @private {boolean}
   * @default true
   */
  castModeHidden_: true,

  /**
   * The current route.
   * @private {?media_router.Route}
   * @default null
   */
  currentRoute_: null,

  /**
   * The current view to be shown.
   * @private {!MediaRouterContainerView}
   * @default MediaRouterContainerView.SINK_PICKER
   */
  currentView_: MediaRouterContainerView.SINK_PICKER,

  /**
   * The previous view that was shown.
   * @private {!MediaRouterContainerView}
   * @default MediaRouterContainerView.SINK_PICKER
   */
  previousView_: MediaRouterContainerView.SINK_PICKER,

  /**
   * Maps media_router.Route.id to corresponding media_router.Route.
   * @private {?Object<!string, !media_router.Route>}
   * @default null
   */
  routeMap_: null,

  /**
   * The value of the selected cast mode in |castModeList|, or -1 if the
   * user has not explicitly selected a mode.
   * @private {number}
   * @default -1
   */
  selectedCastModeValue_: -1,

  /**
   * Maps media_router.Sink.id to corresponding media_router.Sink.
   * @private {?Object<!string, !media_router.Sink>}
   * @default null
   */
  sinkMap_: null,

  /**
   * Maps media_router.Sink.id to corresponding media_router.Route.
   * @private {?Object<!string, ?media_router.Route>}
   * @default null
   */
  sinkToRouteMap_: null,

  /**
   * Adds |route| to |routeList|.
   *
   * @param {!media_router.Route} route The route to add.
   */
  addRoute: function(route) {
    // Check if |route| already exists or if its associated sink
    // does not exist.
    if (this.routeMap_[route.id] || !this.sinkMap_[route.sinkId])
      return;

    // If there is an existing route associated with the same sink, its
    // |sinkToRouteMap_| entry will be overwritten with that of the new route,
    // which results in the correct sink to route mapping.
    this.routeList.push(route);
  },

  /**
   * Filter that returns whether or not the cast modes should be hidden.
   *
   * @param {{castModeHidden: boolean}} value The parameters passed into this
   *     filter.
   * Parameters in |value|:
   *   castModeHidden - Whether or not the cast modes are currently hidden.
   */
  isCastModeHidden: function(value) {
    return value['castModeHidden'];
  },

  /**
   * Filter that returns whether or not the header should be hidden.
   *
   * @param {{state: !MediaRouterContainerView, castModeHidden: boolean}} value
   *     The parameters passed into this filter.
   * Parameters in |value|:
   *   state - The current state of media-router-container.
   *   castModeHidden - Whether or not the cast modes are currently hidden.
   */
  isHeaderHidden: function(value) {
    return value['state'] != MediaRouterContainerView.SINK_PICKER &&
        value['castModeHidden'];
  },

  /**
   * Filter that returns whether or not the issue banner should be hidden.
   *
   * @param {{state: !MediaRouterContainerView,
   *          issue: !media_router.Issue,
   *          castModeHidden: boolean}}
   *     value The parameters passed into this filter.
   * Parameters in |value|:
   *   state - The current state of media-router-container.
   *   issue - The current value of |issue|.
   *   castModeHidden - Whether or not the cast modes are currently hidden.
   */
  isIssueBannerHidden: function(value) {
    return (!value['issue'] &&
        value['state'] != MediaRouterContainerView.BLOCKING_ISSUE) ||
        !value['castModeHidden'];
  },

  /**
   * Filter that returns whether or not the route details should be hidden.
   *
   * @param {{state: !MediaRouterContainerView}} value The parameters passed
   *     into this filter.
   * Parameters in |value|:
   *   state - The current state of media-router-container.
   */
  isRouteDetailsHidden: function(value) {
    return value['state'] != MediaRouterContainerView.ROUTE_DETAILS;
  },

  /**
   * Filter that returns whether or not the sink picker should be hidden.
   *
   * @param {{state: !MediaRouterContainerView, castModeHidden: boolean}} value
   *     The parameters passed into this filter.
   * Parameters in |value|:
   *   state - The current state of media-router-container.
   *   castModeHidden - Whether or not the cast modes are currently hidden.
   */
  isSinkPickerHidden: function(value) {
    return value['state'] != MediaRouterContainerView.SINK_PICKER ||
        !value['castModeHidden'];
  },

  /**
   * Creates a new route if |route| is null.
   *
   * @param {!media_router.Sink} sink The sink to use.
   * @param {!media_router.Route} route The current route tied to |sink|.
   */
  maybeCreateRoute: function(sink, route) {
    if (route) {
      this.showRouteDetailsView();
    } else {
      this.fire('create-route', {
        sinkId: sink.id,
        selectedCastModeValue: this.selectedCastModeValue_
      });
    }
  },

  /**
   * Filter that returns the arrow-drop-* icon to show.
   *
   * @param {{castModeHidden: boolean}} value The parameters passed into this
   *   filter.
   * Parameters in |value|:
   *   castModeHidden - Whether or not the cast mode is currently hidden.
   */
  getDropDownIcon: function(value) {
    return value['castModeHidden'] ? 'arrow-drop-down' : 'arrow-drop-up';
  },

  /**
   * Hides cast-mode-picker.
   */
  hideCastMode: function() {
    this.castModeHidden_ = true;
  },

  /**
   * Called when |issue| has changed. Shows issue-banner if there exists an
   * issue to show. If |newValue| is null, then show the previous view.
   *
   * @param {?media_router.Issue} oldValue The previous value for |issue|.
   * @param {?media_router.Issue} newValue The new value for |issue|.
   */
  issueChanged: function(oldValue, newValue) {
    if (newValue) {
      // Checks that |newValue| is blocking. Also checks that previous issue
      // either did not exist or was not a blocking issue.
      if (newValue.isBlocking && (!oldValue || !oldValue.isBlocking)) {
        this.previousView_ = this.currentView_;
        this.currentView_ = MediaRouterContainerView.BLOCKING_ISSUE;
      }
    } else {
      // Return to the previous view.
      this.currentView_ = this.previousView_;
    }
  },

  /**
   * Fires a close-button-click event. Called when the close button is clicked.
   */
  closeButtonClicked: function() {
    this.fire('close-button-click');
  },

  /**
   * Updates |headerText| and |selectedCastModeValue_|. This is called when one
   * of the cast modes has been selected.
   *
   * @param {!Event} event The event object.
   * @param {!Object} detail The details of the event.
   * @param {!Element} sender Reference to clicked node.
   */
  onCastModeSelected: function(event, detail, sender) {
    var clickedMode = event.target.templateInstance.model.mode;
    this.headerText = clickedMode.title;
    this.selectedCastModeValue_ = clickedMode.type;
  },

  /**
   * Called when a sink is clicked. Updates |currentRoute_|.
   *
   * @param {!Event} event The event object.
   * @param {Object} detail The details of the event.
   * @param {!Element} sender Reference to clicked node.
   */
  onSinkClick: function(event, detail, sender) {
    var clickedSink = event.target.templateInstance.model.sink;
    this.currentRoute_ = this.sinkToRouteMap_[clickedSink.id];
    this.maybeCreateRoute(clickedSink, this.currentRoute_);
  },

  /**
   * Called when |routeList| is updated. Rebuilds |routeMap_| and
   * |sinkToRouteMap_|.
   */
  rebuildRouteMaps: function() {
    // Reset |routeMap_| and |sinkToRouteMap_|.
    this.routeMap_ = {};
    this.sinkToRouteMap_ = {};

    // Rebuild |routeMap_| and |sinkToRouteMap_|.
    this.routeList.forEach(function(route) {
      this.routeMap_[route.id] = route;
      this.sinkToRouteMap_[route.sinkId] = route;
    }, this);
  },

  /**
   * Called when |sinkList| is updated. Rebuilds |sinkMap_|.
   */
  rebuildSinkMap: function() {
    // Reset |sinkMap_|.
    this.sinkMap_ = {};

    // Rebuild |sinkMap_|.
    this.sinkList.forEach(function(sink) {
      this.sinkMap_[sink.id] = sink;
    }, this);
  },

  /**
   * Updates |currentView_| to ROUTE_DETAILS.
   */
  showRouteDetailsView: function() {
    this.previousView_ = this.currentView_;
    this.currentView_ = MediaRouterContainerView.ROUTE_DETAILS;
  },

  /**
   * Updates |currentView_| to SINK_PICKER.
   */
  showSinkPickerView: function() {
    this.previousView_ = this.currentView_;
    this.currentView_ = MediaRouterContainerView.SINK_PICKER;
  },

  /**
   * Toggles |castModeHidden_|.
   */
  toggleCastMode: function() {
    this.castModeHidden_ = !this.castModeHidden_;
  },
});
})();
