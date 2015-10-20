// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This Polymer element contains the entire media router interface. It handles
// hiding and showing specific components.
Polymer({
  is: 'media-router-container',

  properties: {
    /**
     * The list of available sinks.
     * @type {!Array<!media_router.Sink>}
     */
    allSinks: {
      type: Array,
      value: [],
      observer: 'reindexSinksAndRebuildSinksToShow_',
    },

    /**
     * The list of CastModes to show.
     * @private {!Array<!media_router.CastMode>}
     */
    castModeList_: {
      type: Array,
      value: [],
      observer: 'checkCurrentCastMode_',
    },

    /**
     * The ID of the Sink currently being launched.
     * @private {string}
     */
    currentLaunchingSinkId_: {
      type: String,
      value: '',
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
     * @private {?media_router.MediaRouterView}
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
      readOnly: true,
      value: loadTimeData.getString('deviceMissing'),
    },

    /**
     * The URL to open when the device missing link is clicked.
     * @type {string}
     */
    deviceMissingUrl: {
      type: String,
      value: '',
    },

    /**
     * The header text for the sink list.
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
     * The header text when the cast mode list is shown.
     * @private {string}
     */
    selectCastModeHeaderText_: {
      type: String,
      readOnly: true,
      value: loadTimeData.getString('selectCastModeHeader'),
    },

    /**
     * The value of the selected cast mode in |castModeList_|.
     * @private {number}
     */
    selectedCastModeValue_: {
      type: Number,
    },

    /**
     * The subheading text for the non-cast-enabled app cast mode list.
     * @private {string}
     */
    shareYourScreenSubheadingText_: {
      type: String,
      readOnly: true,
      value: loadTimeData.getString('shareYourScreenSubheading'),
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
     * @private {!Object<!string, !media_router.Route>}
     */
    sinkToRouteMap_: {
      type: Object,
      value: {},
    },

    /**
     * Sinks to show for the currently selected cast mode.
     * @private {!Array<!media_router.Sink>}
     */
    sinksToShow_: {
      type: Array,
      value: [],
    },
  },

  ready: function() {
    this.addEventListener('arrow-drop-click', this.toggleCastModeHidden_);
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
   * Checks that the currently selected cast mode is still in the
   * updated list of available cast modes. If not, then update the selected
   * cast mode to the first available cast mode on the list.
   */
  checkCurrentCastMode_: function() {
    if (this.castModeList_.length > 0 &&
        !this.findCastModeByType_(this.selectedCastModeValue_)) {
      this.setSelectedCastMode_(this.castModeList_[0]);
    }
  },

  /**
   * @param {media_router.MediaRouterView} view The current view.
   * @return {boolean} Whether or not to hide the cast mode list.
   * @private
   */
  computeCastModeHidden_: function(view) {
    return view != media_router.MediaRouterView.CAST_MODE_LIST;
  },

  /**
   * @param {!media_router.CastMode} castMode The cast mode to determine an
   *     icon for.
   * @return {string} The Polymer <iron-icon> icon to use. The format is
   *     <iconset>:<icon>, where <iconset> is the set ID and <icon> is the name
   *     of the icon. <iconset>: may be omitted if <icon> is from the default
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
   * @param {media_router.MediaRouterView} view The current view.
   * @param {?media_router.Issue} issue The current issue.
   * @return {boolean} Whether or not to hide the header.
   * @private
   */
  computeHeaderHidden_: function(view, issue) {
    return view == media_router.MediaRouterView.ROUTE_DETAILS ||
        (view == media_router.MediaRouterView.SINK_LIST &&
         issue && issue.isBlocking);
  },

  /**
   * @param {media_router.MediaRouterView} view The current view.
   * @param {string} headerText The header text for the sink list.
   * @return {string} The text for the header.
   * @private
   */
  computeHeaderText_: function(view, headerText) {
    switch (view) {
      case media_router.MediaRouterView.SINK_LIST:
        return this.headerText;
      case media_router.MediaRouterView.CAST_MODE_LIST:
        return this.selectCastModeHeaderText_;
      default:
        return '';
    }
  },

  /**
   * @param {media_router.MediaRouterView} view The current view.
   * @param {string} headerTooltip The tooltip for the header for the sink
   *     list.
   * @return {string} The tooltip for the header.
   * @private
   */
  computeHeaderTooltip_: function(view, headerTooltip) {
    return view == media_router.MediaRouterView.SINK_LIST ? headerTooltip : '';
  },

  /**
   * @param {string} The ID of the sink that is currently launching, or empty
   *     string if none exists.
   * @private
   */
  computeIsLaunching_: function(currentLaunchingSinkId) {
    return currentLaunchingSinkId != '';
  },

  /**
   * @param {?media_router.Issue} issue The current issue.
   * @return {string} The class for the issue banner.
   * @private
   */
  computeIssueBannerClass_: function(issue) {
    return issue && !issue.isBlocking ? 'non-blocking' : '';
  },

  /**
   * @param {media_router.MediaRouterView} view The current view.
   * @param {?media_router.Issue} issue The current issue.
   * @return {boolean} Whether or not to hide the issue banner.
   * @private
   */
  computeIssueBannerHidden_: function(view, issue) {
    return !issue || view == media_router.MediaRouterView.CAST_MODE_LIST;
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
   * @param {media_router.MediaRouterView} view The current view.
   * @param {?media_router.Issue} issue The current issue.
   * @return {boolean} Whether or not to hide the route details.
   * @private
   */
  computeRouteDetailsHidden_: function(view, issue) {
    return view != media_router.MediaRouterView.ROUTE_DETAILS ||
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
   * @return {string} The description value of the route associated with
   *     |sinkId|.
   * @private
   */
  computeRouteInSinkListValue_: function(sinkId, sinkToRouteMap) {
    var route = sinkToRouteMap[sinkId];
    return route ? route.description : '';
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
   * @param {!media_router.Sink} sink The sink to determine an icon for.
   * @return {string} The Polymer <iron-icon> icon to use. The format is
   *     <iconset>:<icon>, where <iconset> is the set ID and <icon> is the name
   *     of the icon. <iconset>: may be ommitted if <icon> is from the default
   *     set.
   * @private
   */
  computeSinkIcon_: function(sink) {
    switch (sink.iconType) {
      case media_router.SinkIconType.CAST:
        return 'hardware:tv';
      case media_router.SinkIconType.CAST_AUDIO:
        return 'hardware:speaker';
      case media_router.SinkIconType.CAST_AUDIO_GROUP:
        return 'hardware:speaker-group';
      case media_router.SinkIconType.GENERIC:
        return 'hardware:tv';
      case media_router.SinkIconType.HANGOUT:
        return 'communication:message';
      default:
        return 'hardware:tv';
    }
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
   * @param {!string} currentLauchingSinkid The ID of the sink that is
   *     currently launching.
   * @param {!string} sinkId A sink ID.
   * @private
   */
  computeSinkIsLaunching_: function(currentLaunchingSinkId, sinkId) {
    return currentLaunchingSinkId == sinkId;
  },

  /**
   * @param {!Array<!media_router.Sink>} The list of sinks.
   * @return {boolean} Whether or not to hide the sink list.
   * @private
   */
  computeSinkListHidden_: function(sinksToShow) {
    return sinksToShow.length == 0;
  },

  /**
   * @param {media_router.MediaRouterView} view The current view.
   * @param {?media_router.Issue} issue The current issue.
   * @return {boolean} Whether or not to hide entire the sink list view.
   * @private
   */
  computeSinkListViewHidden_: function(view, issue) {
    return view != media_router.MediaRouterView.SINK_LIST ||
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
   * Helper function to locate the CastMode object with the given type in
   * castModeList.
   *
   * @param {number} castModeType Type of cast mode to look for.
   * @return {?media_router.CastMode} CastMode object with the given type in
   *     castModeList, or undefined if not found.
   */
  findCastModeByType_: function(castModeType) {
    return this.castModeList_.find(function(element, index, array) {
      return element.type == castModeType;
    });
  },

  /**
   * Sets the list of available cast modes and the initial cast mode.
   *
   * @param {!Array<!media_router.CastMode>} availableCastModes The list
   *     of available cast modes.
   * @param {number} initialCastModeType The initial cast mode when dialog is
   *     opened.
   */
  initializeCastModes: function(availableCastModes, initialCastModeType) {
    this.castModeList_ = availableCastModes;
    var castMode = this.findCastModeByType_(initialCastModeType);
    if (!castMode)
      return;

    this.setSelectedCastMode_(castMode);
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

    this.setSelectedCastMode_(clickedMode);
    this.showSinkList_();
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
    this.currentLaunchingSinkId_ = '';
    // The provider will handle sending an issue for a failed route request.
    if (!route)
      return;

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

  onNotifyRouteCreationTimeout: function() {
    this.currentLaunchingSinkId_ = '';
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

    // We expect that each route in |routeList| maps to a unique sink.
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

    // If |currentRoute_| is no longer active, clear |currentRoute_|. Also
    // switch back to the SINK_PICKER view if the user is currently in the
    // ROUTE_DETAILS view.
    if (!this.currentRoute_ || !this.routeMap_[this.currentRoute_.id]) {
      if (this.currentView_ == media_router.MediaRouterView.ROUTE_DETAILS)
        this.showSinkList_();
      else
        this.currentRoute_ = null;
    }

    this.sinkToRouteMap_ = tempSinkToRouteMap;
    this.maybeShowRouteDetailsOnOpen_(localRoute);
    this.rebuildSinksToShow_();
  },

  /**
   * Rebuilds the list of sinks to be shown for the current cast mode.
   * A sink should be shown if it is compatible with the current cast mode, or
   * if the sink is associated with a route.
   */
  rebuildSinksToShow_: function() {
    var sinksToShow = [];
    this.allSinks.forEach(function(element, index, array) {
      if (element.castModes.indexOf(this.selectedCastModeValue_) != -1 ||
          this.sinkToRouteMap_[element.id]) {
        sinksToShow.push(element);
      }
    }, this);
    this.sinksToShow_ = sinksToShow;
  },

  /**
   * Called when |allSinks| is updated.
   *
   * @private
   */
  reindexSinksAndRebuildSinksToShow_: function() {
    this.sinkMap_ = {};

    this.allSinks.forEach(function(sink) {
      this.sinkMap_[sink.id] = sink;
    }, this);

    this.rebuildSinksToShow_();
  },

  /**
   * Updates the selected cast mode, and updates the header text fields
   * according to the cast mode.
   *
   * @param {!media_router.CastMode} castMode
   */
  setSelectedCastMode_: function(castMode) {
    if (castMode.type != this.selectedCastModeValue_) {
      this.headerText = castMode.description;
      this.headerTextTooltip = castMode.host;
      this.selectedCastModeValue_ = castMode.type;
      this.rebuildSinksToShow_();
    }
  },

  /**
   * Shows the cast mode list.
   *
   * @private
   */
  showCastModeList_: function() {
    this.currentView_ = media_router.MediaRouterView.CAST_MODE_LIST;
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
    } else if (this.currentLaunchingSinkId_ == '') {
      // Allow one launch at a time.
      this.fire('create-route', {
        sinkId: sink.id,
        selectedCastModeValue: this.selectedCastModeValue_
      });
      this.currentLaunchingSinkId_ = sink.id;
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
    this.currentView_ = media_router.MediaRouterView.ROUTE_DETAILS;
  },

  /**
   * Shows the sink list.
   *
   * @private
   */
  showSinkList_: function() {
    this.currentView_ = media_router.MediaRouterView.SINK_LIST;
  },

  /**
   * Toggles |currentView_| between CAST_MODE_LIST and SINK_LIST.
   *
   * @private
   */
  toggleCastModeHidden_: function() {
    if (this.currentView_ == media_router.MediaRouterView.CAST_MODE_LIST)
      this.showSinkList_();
    else
      this.showCastModeList_();
  },
});
