// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  /**
   * The possible states of media-router-container. Used to determine which
   * components of media-router-container to show.
   *
   * @enum {string}
   */
  var MediaRouterContainerView = {
    CAST_MODE_LIST: 'cast-mode-list',
    FILTER: 'filter',
    ROUTE_DETAILS: 'route-details',
    SINK_LIST: 'sink-list',
  };

// This Polymer element contains the entire media router interface. It handles
// hiding and showing specific components.
Polymer({
  is: 'media-router-container',

  properties: {
    /**
     * The list of CastModes to show.
     * @type {!Array<!media_router.CastMode>}
     */
    castModeList: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * The current route.
     * @private {?media_router.Route}
     */
    currentRoute_: {
      type: Object,
      value: null,
    },

    /**
     * The current view to be shown.
     * @private {!MediaRouterContainerView}
     */
    currentView_: {
      type: String,
      value: MediaRouterContainerView.SINK_LIST,
    },

    /**
     * The header text.
     * @type {string}
     */
    headerText: {
      type: String,
      value: '',
    },

    /**
     * The issue to show.
     * @type {?media_router.Issue}
     */
    issue: {
      type: Object,
      value: null,
    },

    /**
     * The list of current routes.
     * @type {!Array<!media_router.Route>}
     */
    routeList: {
      type: Array,
      value: function() {
        return [];
      },
      observer: 'rebuildRouteMaps_',
    },

    /**
     * Maps media_router.Route.id to corresponding media_router.Route.
     * @private {!Object<!string, !media_router.Route>}
     */
    routeMap_: {
      type: Object,
      value: function() {
        return {};
      },
    },

    /**
     * The header text when the cast mode list is shown.
     * @private {string}
     */
    selectCastModeHeaderText_: {
      type: String,
      value: function() {
        return loadTimeData.getString('selectCastModeHeader');
      },
    },

    /**
     * The value of the selected cast mode in |castModeList|, or -1 if the
     * user has not explicitly selected a cast mode.
     * @private {number}
     */
    selectedCastModeValue_: {
      type: Number,
      value: -1,
      observer: 'showSinkList_',
    },

    /**
     * The list of available sinks.
     * @type {!Array<!media_router.Sink}
     */
    sinkList: {
      type: Array,
      value: function() {
        return [];
      },
      observer: 'rebuildSinkMap_',
    },

    /**
     * Maps media_router.Sink.id to corresponding media_router.Sink.
     * @private {!Object<!string, !media_router.Sink>}
     */
    sinkMap_: {
      type: Object,
      value: function() {
        return {};
      },
    },

    /**
     * Maps media_router.Sink.id to corresponding media_router.Route.
     * @private {!Object<!string, ?media_router.Route>}
     */
    sinkToRouteMap_: {
      type: Object,
      value: function() {
        return {};
      },
    },
  },

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
   * @param {!MediaRouterContainerView} view The current view.
   * @return {string} The current arrow-drop-* icon to use.
   * @private
   */
  computeArrowDropIcon_: function(view) {
    return view == MediaRouterContainerView.CAST_MODE_LIST ?
        'arrow-drop-up' : 'arrow-drop-down';
  },

  /**
   * @param {!MediaRouterContainerView} view The current view.
   * @return {boolean} Whether or not to hide the cast mode list.
   * @private
   */
  computeCastModeHidden_: function(view) {
    return view != MediaRouterContainerView.CAST_MODE_LIST;
  },

  /**
   * @param {!MediaRouterContainerView} view The current view.
   * @param {?media_router.Issue} issue The current issue.
   * @return {boolean} Whether or not to hide the header.
   * @private
   */
  computeHeaderHidden_: function(view, issue) {
    return view == MediaRouterContainerView.ROUTE_DETAILS ||
        (view == MediaRouterContainerView.SINK_LIST &&
         issue && issue.isBlocking);
  },

  /**
   * @param {!MediaRouterContainerView} view The current view.
   * @param {?media_router.Issue} issue The current issue.
   * @return {boolean} Whether or not to hide the issue banner.
   * @private
   */
  computeIssueBannerHidden_: function(view, issue) {
    return !issue || view == MediaRouterContainerView.CAST_MODE_LIST;
  },

  /**
   * @param {!MediaRouterContainerView} view The current view.
   * @param {?media_router.Issue} issue The current issue.
   * @return {boolean} Whether or not to hide the route details.
   * @private
   */
  computerRouteDetailsHidden_: function(view, issue) {
    return view != MediaRouterContainerView.ROUTE_DETAILS ||
        (issue && issue.isBlocking);
  },

  /**
   * @param {!string} sinkId A sink ID.
   * @return {boolean} Whether or not to hide the route info in the sink list
   *     that is associated with |sinkId|.
   * @private
   */
  computeRouteInSinkListHidden_: function(sinkId) {
    return !this.sinkToRouteMap_[sinkId];
  },

  /**
   * @param {!string} sinkId A sink ID.
   * @return {string} The title value of the route associated with |sinkId|.
   * @private
   */
  computeRouteInSinkListValue_: function(sinkId) {
    var route = this.sinkToRouteMap_[sinkId];
    return route ? this.sinkToRouteMap_[sinkId].title : '';
  },

  /**
   * @param {?media_router.Route} route The current route.
   * @return {?media_router.Sink} The sink associated with |route|.
   * @private
   */
  computeSinkForCurrentRoute_: function(route) {
    return route ? this.sinkMap_[route.sinkId] : null;
  },

  /**
   * @param {!MediaRouterContainerView} view The current view.
   * @param {?media_router.Issue} issue The current issue.
   * @return {boolean} Whether or not to hide the sink list.
   * @private
   */
  computeSinkListHidden_: function(view, issue) {
    return view != MediaRouterContainerView.SINK_LIST ||
        (issue && issue.isBlocking);
  },

  /**
   * Creates a new route if |route| is null. Otherwise, shows the route
   * details.
   *
   * @param {!media_router.Sink} sink The sink to use.
   * @param {?media_router.Route} route The current route tied to |sink|.
   * @private
   */
  showOrCreateRoute_: function(sink, route) {
    if (route) {
      this.showRouteDetails_();
    } else {
      this.fire('create-route', {
        sinkId: sink.id,
        selectedCastModeValue: this.selectedCastModeValue_
      });
    }
  },

  /**
   * Handles a cast mode selection. Updates |headerText| and
   * |selectedCastModeValue_|.
   *
   * @param {!Event} event The event object.
   * @private
   */
  onCastModeClick_: function(event) {
    var clickedMode = this.$.castModeList.itemForElement(event.target);
    this.headerText = clickedMode.title;
    this.selectedCastModeValue_ = clickedMode.type;
  },

  /**
   * Handles a click on the close button by firing a close-button-click event.
   *
   * @private
   */
  onCloseButtonClick_: function() {
    this.fire('close-button-click');
  },

  /**
   * Called when a sink is clicked. Updates |currentRoute_|.
   *
   * @param {!Event} event The event object.
   * @private
   */
  onSinkClick_: function(event) {
    var clickedSink = this.$.sinkList.itemForElement(event.target);
    this.currentRoute_ = this.sinkToRouteMap_[clickedSink.id];
    this.showOrCreateRoute_(clickedSink, this.currentRoute_);
  },

  /**
   * Called when |routeList| is updated. Rebuilds |routeMap_| and
   * |sinkToRouteMap_|.
   *
   * @private
   */
  rebuildRouteMaps_: function() {
    this.routeMap_ = {};
    this.sinkToRouteMap_ = {};

    this.routeList.forEach(function(route) {
      this.routeMap_[route.id] = route;
      this.sinkToRouteMap_[route.sinkId] = route;
    }, this);
  },

  /**
   * Called when |sinkList| is updated. Rebuilds |sinkMap_|.
   *
   * @private
   */
  rebuildSinkMap_: function() {
    this.sinkMap_ = {};

    this.sinkList.forEach(function(sink) {
      this.sinkMap_[sink.id] = sink;
    }, this);
  },

  /**
   * Shows the cast mode list.
   *
   * @private
   */
  showCastModeList_: function() {
    this.currentView_ = MediaRouterContainerView.CAST_MODE_LIST;
  },

  /**
   * Shows the route details.
   *
   * @private
   */
  showRouteDetails_: function() {
    this.currentView_ = MediaRouterContainerView.ROUTE_DETAILS;
  },

  /**
   * Shows the sink list.
   *
   * @private
   */
  showSinkList_: function() {
    this.currentView_ = MediaRouterContainerView.SINK_LIST;
  },

  /**
   * Toggles |currentView_| between CAST_MODE_LIST and SINK_LIST.
   *
   * @private
   */
  toggleCastModeHidden_: function() {
    if (this.currentView_ == MediaRouterContainerView.CAST_MODE_LIST) {
      this.showSinkList_();
    } else {
      this.showCastModeList_();
    }
  },
});
})();
