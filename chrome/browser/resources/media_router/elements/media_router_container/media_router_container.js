// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
      value: [],
    },

    /**
     * The possible states of media-router-container. Used to determine which
     * components of media-router-container to show.
     * This is a property of media-router-container because it is used in
     * tests.
     *
     * @enum {string}
     * @private
     */
    CONTAINER_VIEW_: {
      type: Object,
      readOnly: true,
      value: {
        CAST_MODE_LIST: 'cast-mode-list',
        FILTER: 'filter',
        ROUTE_DETAILS: 'route-details',
        SINK_LIST: 'sink-list',
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
     * @private {CONTAINER_VIEW_}
     */
    currentView_: {
      type: String,
      value: '',
    },

    /**
     * The text for when there are no devices.
     * @private {string}
     */
    deviceMissingText_: {
      type: String,
      value: loadTimeData.getString('deviceMissing'),
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
     * The header text tooltip. This would be descriptive of the
     * source origin, whether a host name, tab URL, etc.
     * @type {string}
     */
    headerTextTooltip: {
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
     * Whether the MR UI was just opened.
     * @private {boolean}
     */
    justOpened_: {
      type: Boolean,
      value: true,
      observer: 'computeSpinnerHidden_',
    },

    /**
     * The number of current local routes.
     * @private {number}
     */
    localRouteCount_: {
      type: Number,
      value: 0,
    },

    /**
     * The list of current routes.
     * @type {!Array<!media_router.Route>}
     */
    routeList: {
      type: Array,
      value: [],
      observer: 'rebuildRouteMaps_',
    },

    /**
     * Maps media_router.Route.id to corresponding media_router.Route.
     * @private {!Object<!string, !media_router.Route>}
     */
    routeMap_: {
      type: Object,
      value: {},
    },

    /**
     * The ID of the media route provider extension.
     * @type {string}
     */
    routeProviderExtensionId: {
      type: String,
      value: '',
      observer: 'propogateExtensionId_',
    },

    /**
     * The header text when the cast mode list is shown.
     * @private {string}
     */
    selectCastModeHeaderText_: {
      type: String,
      value: loadTimeData.getString('selectCastModeHeader'),
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
     * The subheading text for the non-cast-enabled app cast mode list.
     * @private {string}
     */
    shareYourScreenSubheadingText_: {
      type: String,
      value: loadTimeData.getString('shareYourScreenSubheading'),
    },

    /**
     * The list of available sinks.
     * @type {!Array<!media_router.Sink>}
     */
    sinkList: {
      type: Array,
      value: [],
      observer: 'rebuildSinkMap_',
    },

    /**
     * Maps media_router.Sink.id to corresponding media_router.Sink.
     * @private {!Object<!string, !media_router.Sink>}
     */
    sinkMap_: {
      type: Object,
      value: {},
    },

    /**
     * Maps media_router.Sink.id to corresponding media_router.Route.
     * @private {!Object<!string, ?media_router.Route>}
     */
    sinkToRouteMap_: {
      type: Object,
      value: {},
    },
  },

  ready: function() {
    this.addEventListener('close-route-click', this.removeRoute);
    this.showSinkList_();
  },

  attached: function() {
    // Turn off the spinner after 3 seconds.
    this.async(function() {
      this.justOpened_ = false;
    }, 3000 /* 3 seconds */);
  },

  /**
   * @param {CONTAINER_VIEW_} view The current view.
   * @return {string} The current arrow-drop-* icon to use.
   * @private
   */
  computeArrowDropIcon_: function(view) {
    return view == this.CONTAINER_VIEW_.CAST_MODE_LIST ?
        'arrow-drop-up' : 'arrow-drop-down';
  },

  /**
   * @param {CONTAINER_VIEW_} view The current view.
   * @return {boolean} Whether or not to hide the cast mode list.
   * @private
   */
  computeCastModeHidden_: function(view) {
    return view != this.CONTAINER_VIEW_.CAST_MODE_LIST;
  },

  /**
   * @param {!media_router.CastMode} castMode The cast mode to determine an
   *     icon for.
   * @return {string} The Polymer <iron-icon> icon to use. The format is
   *     <iconset>:<icon>, where <iconset> is the set ID and <icon> is the name
   *     of the icon. <iconset>: may be ommitted if <icon> is from the default
   *     set.
   * @private
   */
  computeCastModeIcon_: function(castMode) {
    switch (castMode.type) {
      case media_router.CastModeType.DEFAULT:
        return 'av:web';
      case media_router.CastModeType.TAB_MIRROR:
        return 'tab';
      case media_router.CastModeType.DESKTOP_MIRROR:
        return 'hardware:laptop';
      default:
        return '';
    }
  },

  /**
   * @param {!Array<!media_router.CastMode>} castModeList The current list of
   *      cast modes.
   * @return {!Array<!media_router.CastMode>} The list of default cast modes.
   * @private
   */
  computeDefaultCastModeList_: function(castModeList) {
    return castModeList.filter(function(mode) {
      return mode.type == media_router.CastModeType.DEFAULT;
    });
  },

  /**
   * @param {CONTAINER_VIEW_} view The current view.
   * @param {?media_router.Issue} issue The current issue.
   * @return {boolean} Whether or not to hide the header.
   * @private
   */
  computeHeaderHidden_: function(view, issue) {
    return view == this.CONTAINER_VIEW_.ROUTE_DETAILS ||
        (view == this.CONTAINER_VIEW_.SINK_LIST &&
         issue && issue.isBlocking);
  },

  /**
   * @param {CONTAINER_VIEW_} view The current view.
   * @param {?media_router.Issue} issue The current issue.
   * @return {boolean} Whether or not to hide the issue banner.
   * @private
   */
  computeIssueBannerHidden_: function(view, issue) {
    return !issue || view == this.CONTAINER_VIEW_.CAST_MODE_LIST;
  },

  /**
   * @param {!Array<!media_router.CastMode>} castModeList The current list of
   *     cast modes.
   * @return {!Array<!media_router.CastMode>} The list of non-default cast
   *     modes.
   * @private
   */
  computeNonDefaultCastModeList_: function(castModeList) {
    return castModeList.filter(function(mode) {
      return mode.type != media_router.CastModeType.DEFAULT;
    });
  },

  /**
   * @param {CONTAINER_VIEW_} view The current view.
   * @param {?media_router.Issue} issue The current issue.
   * @return {boolean} Whether or not to hide the route details.
   * @private
   */
  computeRouteDetailsHidden_: function(view, issue) {
    return view != this.CONTAINER_VIEW_.ROUTE_DETAILS ||
        (issue && issue.isBlocking);
  },

  /**
   * @param {!string} sinkId A sink ID.
   * @return {boolean} Whether or not to hide the route info in the sink list
   *     that is associated with |sinkId|.
   * @private
   */
  computeRouteInSinkListHidden_: function(sinkId, sinkToRouteMap) {
    return !sinkToRouteMap[sinkId];
  },

  /**
   * @param {!string} sinkId A sink ID.
   * @return {string} The title value of the route associated with |sinkId|.
   * @private
   */
  computeRouteInSinkListValue_: function(sinkId, sinkToRouteMap) {
    var route = sinkToRouteMap[sinkId];
    return route ? route.title : '';
  },

  /**
   * @param {!Array<!media_router.CastMode>} castModeList The current list of
   *     cast modes.
   * @return {boolean} Whether or not to hide the share screen subheading text.
   * @private
   */
  computeShareScreenSubheadingHidden_: function(castModeList) {
    return this.computeNonDefaultCastModeList_(castModeList).length == 0;
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
   * @param {!string} sinkId A sink ID.
   * @param {!Object<!string, ?media_router.Route>} sinkToRouteMap
   *     Maps media_router.Sink.id to corresponding media_router.Route.
   * @return {string} The class for the sink icon.
   * @private
   */
  computeSinkIconClass_: function(sinkId, sinkToRouteMap) {
    return sinkToRouteMap[sinkId] ? 'sink-icon active-sink' : 'sink-icon';
  },

  /**
   * @param {!Array<!media_router.Sink>} The list of sinks.
   * @return {boolean} Whether or not to hide the sink list.
   * @private
   */
  computeSinkListHidden_: function(sinkList) {
    return sinkList.length == 0;
  },

  /**
   * @param {CONTAINER_VIEW_} view The current view.
   * @param {?media_router.Issue} issue The current issue.
   * @return {boolean} Whether or not to hide entire the sink list view.
   * @private
   */
  computeSinkListViewHidden_: function(view, issue) {
    return view != this.CONTAINER_VIEW_.SINK_LIST ||
        (issue && issue.isBlocking);
  },

  /**
   * @param {boolean} justOpened Whether the MR UI was just opened.
   * @return {boolean} Whether or not to hide the spinner.
   * @private
   */
  computeSpinnerHidden_: function(justOpened) {
    return !justOpened;
  },

  /**
   * Checks if there is a sink whose isLaunching is true.
   *
   * @param {!Array<!media_router.Sink>} sinks
   * @return {boolean}
   * @private
   */
  isLaunching_: function(sinks) {
    return sinks.some(function(sink) { return sink.isLaunching; });
  },

  /**
   * Updates |currentView_| if the dialog had just opened and there's
   * only one local route.
   *
   * @param {?media_router.Route} route A local route.
   * @private
   */
  maybeShowRouteDetailsOnOpen_: function(route) {
    if (this.localRouteCount_ == 1 && this.justOpened_ && route)
      this.showRouteDetails_(route);
  },

  /**
   * Handles a cast mode selection. Updates |headerText|, |headerTextTooltip|,
   * and |selectedCastModeValue_|.
   *
   * @param {!Event} event The event object.
   * @private
   */
  onCastModeClick_: function(event) {
    // The clicked cast mode can come from one of two lists,
    // defaultCastModeList and nonDefaultCastModeList.
    var clickedMode =
        this.$.defaultCastModeList.itemForElement(event.target) ||
            this.$.nonDefaultCastModeList.itemForElement(event.target);

    if (!clickedMode)
      return;

    this.headerText = clickedMode.description;
    this.headerTextTooltip = clickedMode.host;
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
   * Handles response of previous create route attempt.
   *
   * @param {string} sinkId The ID of the sink to which the Media Route was
   *     creating a route.
   * @param {?media_router.Route} route The newly created route to the sink
   *     if succeeded; null otherwise.
   */
  onCreateRouteResponseReceived: function(sinkId, route) {
    this.setLaunchState_(sinkId, false);
    if (!route) {
      // TODO(apacible) Show launch failure.
      return;
    }

    // Check if |route| already exists or if its associated sink
    // does not exist.
    if (this.routeMap_[route.id] || !this.sinkMap_[route.sinkId])
      return;

    // If there is an existing route associated with the same sink, its
    // |sinkToRouteMap_| entry will be overwritten with that of the new route,
    // which results in the correct sink to route mapping.
    this.routeList.push(route);
    this.showRouteDetails_(route);
  },

  /**
   * Called when a sink is clicked.
   *
   * @param {!Event} event The event object.
   * @private
   */
  onSinkClick_: function(event) {
    this.showOrCreateRoute_(this.$.sinkList.itemForElement(event.target));
  },

  /**
   * Propagates extension ID to the child elements that need it.
   *
   * @private
   */
  propogateExtensionId_: function() {
    this.$['route-details'].routeProviderExtensionId =
        this.routeProviderExtensionId;
  },

  /**
   * Called when |routeList| is updated. Rebuilds |routeMap_| and
   * |sinkToRouteMap_|.
   *
   * @private
   */
  rebuildRouteMaps_: function() {
    this.routeMap_ = {};
    this.localRouteCount_ = 0;

    // Keeps track of the last local route we find in |routeList|. If
    // |localRouteCount_| is eventually equal to one, |localRoute| would be the
    // only current local route.
    var localRoute = null;

    // Rebuild |sinkToRouteMap_| with a temporary map to avoid firing the
    // computed functions prematurely.
    var tempSinkToRouteMap = {};

    this.routeList.forEach(function(route) {
      this.routeMap_[route.id] = route;
      tempSinkToRouteMap[route.sinkId] = route;

      if (route.isLocal) {
        this.localRouteCount_++;
        // It's OK if localRoute is updated multiple times; it is only used if
        // |localRouteCount_| == 1, which implies it was only set once.
        localRoute = route;
      }
    }, this);

    this.sinkToRouteMap_ = tempSinkToRouteMap;
    this.maybeShowRouteDetailsOnOpen_(localRoute);
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
   * Temporarily overrides the "isLaunching" bit for a sink.
   *
   * @param {string} sinkId The ID of the sink.
   * @param {boolean} isLaunching Whether or not the media router is creating
   *    a route to the sink.
   * @private
   */
  setLaunchState_: function(sinkId, isLaunching) {
    for (var index = 0; index < this.sinkList.length; index++) {
      if (this.sinkList[index].id == sinkId) {
        this.set(['sinkList', index, 'isLaunching'], isLaunching);
        return;
      }
    }
  },

  /**
   * Shows the cast mode list.
   *
   * @private
   */
  showCastModeList_: function() {
    this.currentRoute_ = null;
    this.currentView_ = this.CONTAINER_VIEW_.CAST_MODE_LIST;
  },

  /**
   * Creates a new route if there is no route to the |sink| . Otherwise,
   * shows the route details.
   *
   * @param {!media_router.Sink} sink The sink to use.
   * @private
   */
  showOrCreateRoute_: function(sink) {
    var route = this.sinkToRouteMap_[sink.id];
    if (route) {
      this.showRouteDetails_(route);
    } else if (!this.isLaunching_(this.sinkList)) {
      // Allow one launch at a time.
      this.setLaunchState_(sink.id, true);
      this.fire('create-route', {
        sinkId: sink.id,
        selectedCastModeValue: this.selectedCastModeValue_
      });
    }
  },


  /**
   * Shows the route details.
   *
   * @param {!media_router.Route} route The route to show.
   * @private
   */
  showRouteDetails_: function(route) {
    this.currentRoute_ = route;
    this.currentView_ = this.CONTAINER_VIEW_.ROUTE_DETAILS;
  },

  /**
   * Shows the sink list.
   *
   * @private
   */
  showSinkList_: function() {
    this.currentRoute_ = null;
    this.currentView_ = this.CONTAINER_VIEW_.SINK_LIST;
  },

  /**
   * Toggles |currentView_| between CAST_MODE_LIST and SINK_LIST.
   *
   * @private
   */
  toggleCastModeHidden_: function() {
    if (this.currentView_ == this.CONTAINER_VIEW_.CAST_MODE_LIST)
      this.showSinkList_();
    else
      this.showCastModeList_();
  },
});
