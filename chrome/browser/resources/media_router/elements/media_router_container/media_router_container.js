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
     * @type {!Array<!media_router.CastMode>}
     */
    castModeList: {
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
      value: null,
    },

    /**
     * The text for when there are no devices.
     * @private {string}
     */
    deviceMissingText_: {
      type: String,
      readOnly: true,
      value: function() {
        return loadTimeData.getString('deviceMissing');
      },
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
     * The height of the dialog.
     * @private {number}
     */
    dialogHeight_: {
      type: Number,
      value: 330,
    },

    /**
     * The time |this| element calls ready().
     * @private {number}
     */
    elementReadyTimeMs_: {
      type: Number,
      value: 0,
    },

    /**
     * The text for the first run flow button.
     * @private {string}
     */
    firstRunFlowButtonText_: {
      type: String,
      readOnly: true,
      value: function() {
        return loadTimeData.getString('firstRunFlowButton');
      },
    },

    /**
     * The text for the learn more link about cloud services in the first run
     * flow.
     * @private {string}
     */
    firstRunFlowCloudLearnMore_: {
      type: String,
      readOnly: true,
      value: loadTimeData.valueExists('firstRunFlowCloudLearnMore') ?
          loadTimeData.getString('firstRunFlowCloudLearnMore') : '',
    },

    /**
     * The text for the cloud services preference description in the first run
     * flow.
     * @private {string}
     */
    firstRunFlowCloudPrefText_: {
      type: String,
      readOnly: true,
      value: loadTimeData.valueExists('firstRunFlowCloudPrefText') ?
          loadTimeData.getString('firstRunFlowCloudPrefText') : '',
    },

    /**
     * The text description for the first run flow.
     * @private {string}
     */
    firstRunFlowText_: {
      type: String,
      readOnly: true,
      value: function() {
        return loadTimeData.getString('firstRunFlowText');
      },
    },

    /**
     * The header of the first run flow.
     * @private {string}
     */
    firstRunFlowTitle_: {
      type: String,
      readOnly: true,
      value: function() {
        return loadTimeData.getString('firstRunFlowTitle');
      },
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
     * @type {?string}
     */
    headerTextTooltip: {
      type: String,
      value: null,
    },

    /**
     * The issue to show.
     * @type {?media_router.Issue}
     */
    issue: {
      type: Object,
      value: null,
      observer: 'maybeShowIssueView_',
    },

    /**
     * The header text.
     * @private {string}
     */
    issueHeaderText_: {
      type: String,
      readOnly: true,
      value: function() {
        return loadTimeData.getString('issueHeader');
      },
    },

    /**
     * Whether the MR UI was just opened.
     * @private {boolean}
     */
    justOpened_: {
      type: Boolean,
      value: true,
    },

    /**
     * Whether the user's mouse is positioned over the dialog.
     * @private {boolean}
     */
    mouseIsPositionedOverDialog_: {
      type: Boolean,
      value: false,
    },

    /**
     * The time the sink list was shown and populated with at least one sink.
     * This is reset whenever the user switches views or there are no sinks
     * available for display.
     * @private {number}
     */
    populatedSinkListSeenTimeMs_: {
      type: Number,
      value: -1,
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
      value: function() {
        return loadTimeData.getString('selectCastModeHeader');
      },
    },

    /**
     * The subheading text for the non-cast-enabled app cast mode list.
     * @private {string}
     */
    shareYourScreenSubheadingText_: {
      type: String,
      readOnly: true,
      value: function() {
        return loadTimeData.getString('shareYourScreenSubheading');
      },
    },

    /**
     * Whether to show the first run flow.
     * @type {boolean}
     */
    showFirstRunFlow: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether to show the cloud preference setting in the first run flow.
     * @type {boolean}
     */
    showFirstRunFlowCloudPref: {
      type: Boolean,
      value: false,
    },

    /**
     * The cast mode shown to the user. Initially set to auto mode. (See
     * media_router.CastMode documentation for details on auto mode.)
     * This value may be changed in one of the following ways:
     * 1) The user explicitly selected a cast mode.
     * 2) The user selected cast mode is no longer available for the associated
     *    WebContents. In this case, the container will reset to auto mode. Note
     *    that |userHasSelectedCastMode_| will switch back to false.
     * 3) The sink list changed, and the user had not explicitly selected a cast
     *    mode. If the sinks support exactly 1 cast mode, the container will
     *    switch to that cast mode. Otherwise, the container will reset to auto
     *    mode.
     * @private {number}
     */
    shownCastModeValue_: {
      type: Number,
      value: media_router.AUTO_CAST_MODE.type,
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

    /**
     * Whether the user has explicitly selected a cast mode.
     * @private {boolean}
     */
    userHasSelectedCastMode_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether the user has already taken an action.
     * @type {boolean}
     */
    userHasTakenInitialAction_: {
      type: Boolean,
      value: false,
    },
  },

  listeners: {
    'arrow-drop-click': 'toggleCastModeHidden_',
    'mouseleave': 'onMouseLeave_',
    'mouseenter': 'onMouseEnter_',
  },

  observers: [
    'maybeUpdateStartSinkDisplayStartTime_(currentView_, sinksToShow_)',
    'shownComponentsChanged_(showFirstRunFlow, currentView_)'
  ],

  ready: function() {
    this.elementReadyTimeMs_ = performance.now();
    this.showSinkList_();
  },

  attached: function() {
    this.updateElementPositioning_();

    // Turn off the spinner after 3 seconds, then report the current number of
    // sinks.
    this.async(function() {
      this.justOpened_ = false;
      this.fire('report-sink-count', {
        sinkCount: this.allSinks.length,
      });
    }, 3000 /* 3 seconds */);
  },

  /**
   * Fires an acknowledge-first-run-flow event and hides the first run flow.
   * This is call when the first run flow button is clicked.
   *
   * @private
   */
  acknowledgeFirstRunFlow_: function() {
    this.showFirstRunFlow = false;
    this.fire('acknowledge-first-run-flow');
  },

  /**
   * Fires a 'report-initial-action' event when the user takes their first
   * action after the dialog opens. Also fires a 'report-initial-action-close'
   * event if that initial action is to close the dialog.
   * @param {!media_router.MediaRouterUserAction} initialAction
   */
  maybeReportUserFirstAction: function(initialAction) {
    if (this.userHasTakenInitialAction_)
      return;

    this.fire('report-initial-action', {
      action: initialAction,
    });

    if (initialAction == media_router.MediaRouterUserAction.CLOSE) {
      var timeToClose = performance.now() - this.elementReadyTimeMs_;
      this.fire('report-initial-action-close', {
        timeMs: timeToClose,
      });
    }

    this.userHasTakenInitialAction_ = true;
  },

  /**
   * Checks that the currently selected cast mode is still in the
   * updated list of available cast modes. If not, then update the selected
   * cast mode to the first available cast mode on the list.
   */
  checkCurrentCastMode_: function() {
    if (!this.castModeList.length)
      return;

    // If we are currently showing auto mode, then nothing needs to be done.
    // Otherwise, if the cast mode currently shown no longer exists (regardless
    // of whether it was selected by user), then switch back to auto cast mode.
    if (this.shownCastModeValue_ != media_router.CastModeType.AUTO &&
        !this.findCastModeByType_(this.shownCastModeValue_)) {
      this.setShownCastMode_(media_router.AUTO_CAST_MODE);
      this.rebuildSinksToShow_();
    }
  },

  /**
   * If |allSinks| supports only a single cast mode, returns that cast mode.
   * Otherwise, returns AUTO_MODE. Only called if |userHasSelectedCastMode_| is
   * |false|.
   * @return {!media_router.CastMode} The single cast mode supported by
   *                                  |allSinks|, or AUTO_MODE.
   */
  computeCastMode_: function() {
    var allCastModes = this.allSinks.reduce(function(castModesSoFar, sink) {
      return castModesSoFar | sink.castModes;
    }, 0);

    // This checks whether |castModes| does not consist of exactly 1 cast mode.
    if (!allCastModes || allCastModes & (allCastModes - 1))
      return media_router.AUTO_CAST_MODE;

    var castMode = this.findCastModeByType_(allCastModes);
    if (castMode)
      return castMode;

    console.error('Cast mode ' + allCastModes + ' not in castModeList');
    return media_router.AUTO_CAST_MODE;
  },

  /**
   * @param {?media_router.MediaRouterView} view The current view.
   * @return {boolean} Whether or not to hide the cast mode list.
   * @private
   */
  computeCastModeListHidden_: function(view) {
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
   * @param {?media_router.MediaRouterView} view The current view.
   * @param {?media_router.Issue} issue The current issue.
   * @return {boolean} Whether or not to hide the header.
   * @private
   */
  computeHeaderHidden_: function(view, issue) {
    return view == media_router.MediaRouterView.ROUTE_DETAILS ||
        (view == media_router.MediaRouterView.SINK_LIST &&
         !!issue && issue.isBlocking);
  },

  /**
   * @param {?media_router.MediaRouterView} view The current view.
   * @param {string} headerText The header text for the sink list.
   * @return {string} The text for the header.
   * @private
   */
  computeHeaderText_: function(view, headerText) {
    switch (view) {
      case media_router.MediaRouterView.CAST_MODE_LIST:
        return this.selectCastModeHeaderText_;
      case media_router.MediaRouterView.ISSUE:
        return this.issueHeaderText_;
      case media_router.MediaRouterView.ROUTE_DETAILS:
        return this.currentRoute_ ?
            this.sinkMap_[this.currentRoute_.sinkId].name : '';
      case media_router.MediaRouterView.SINK_LIST:
        return this.headerText;
      default:
        return '';
    }
  },

  /**
   * @param {?media_router.MediaRouterView} view The current view.
   * @param {string} headerTooltip The tooltip for the header for the sink
   *     list.
   * @return {string} The tooltip for the header.
   * @private
   */
  computeHeaderTooltip_: function(view, headerTooltip) {
    return view == media_router.MediaRouterView.SINK_LIST ? headerTooltip : '';
  },

  /**
   * @param {string} currentLaunchingSinkId ID of the sink that is currently
   *     launching, or empty string if none exists.
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
    return !!issue && !issue.isBlocking ? 'non-blocking' : '';
  },

  /**
   * @param {?media_router.MediaRouterView} view The current view.
   * @param {?media_router.Issue} issue The current issue.
   * @return {boolean} Whether or not to show the issue banner.
   * @private
   */
  computeIssueBannerShown_: function(view, issue) {
    return !!issue && (view == media_router.MediaRouterView.SINK_LIST ||
                       view == media_router.MediaRouterView.ISSUE);
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
   * @param {?media_router.MediaRouterView} view The current view.
   * @param {?media_router.Issue} issue The current issue.
   * @return {boolean} Whether or not to hide the route details.
   * @private
   */
  computeRouteDetailsHidden_: function(view, issue) {
    return view != media_router.MediaRouterView.ROUTE_DETAILS ||
        (!!issue && issue.isBlocking);
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
   * @param {boolean} showFirstRunFlow Whether or not to show the first run
   *     flow.
   * @param {?media_router.MediaRouterView} currentView The current view.
   * @private
   */
  computeShowFirstRunFlow_: function(showFirstRunFlow, currentView) {
    return showFirstRunFlow &&
        currentView == media_router.MediaRouterView.SINK_LIST;
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
   * @param {!string} currentLaunchingSinkId The ID of the sink that is
   *     currently launching.
   * @param {!string} sinkId A sink ID.
   * @return {boolean} |true| if given sink is currently launching.
   * @private
   */
  computeSinkIsLaunching_: function(currentLaunchingSinkId, sinkId) {
    return currentLaunchingSinkId == sinkId;
  },

  /**
   * @param {!Array<!media_router.Sink>} sinksToShow The list of sinks.
   * @return {boolean} Whether or not to hide the sink list.
   * @private
   */
  computeSinkListHidden_: function(sinksToShow) {
    return sinksToShow.length == 0;
  },

  /**
   * @param {?media_router.MediaRouterView} view The current view.
   * @param {?media_router.Issue} issue The current issue.
   * @return {boolean} Whether or not to hide entire the sink list view.
   * @private
   */
  computeSinkListViewHidden_: function(view, issue) {
    return view != media_router.MediaRouterView.SINK_LIST ||
        (!!issue && issue.isBlocking);
  },

  /**
   * Returns whether the sink domain for |sink| should be hidden.
   * @param {!media_router.Sink} sink
   * @return {boolean} |true| if the domain should be hidden.
   * @private
   */
  computeSinkDomainHidden_: function(sink) {
    // TODO(amp): Check the domain of Chrome profile identity and only show the
    // sink domain if it doesn't match the profile domain. crbug.com/570797
    return this.isEmptyOrWhitespace_(sink.domain);
  },

  /**
   * Returns the subtext to be shown for |sink|. Only called if
   * |computeSinkSubtextHidden_| returns false for the same |sink| and
   * |sinkToRouteMap|.
   * @param {!media_router.Sink} sink
   * @param {!Object<!string, ?media_router.Route>} sinkToRouteMap
   * @return {?string} The subtext to be shown.
   * @private
   */
  computeSinkSubtext_: function(sink, sinkToRouteMap) {
    var route = sinkToRouteMap[sink.id];
    if (route && !this.isEmptyOrWhitespace_(route.description))
      return route.description;

    return sink.description;
  },

  /**
   * Returns whether the sink subtext for |sink| should be hidden.
   * @param {!media_router.Sink} sink
   * @param {!Object<!string, ?media_router.Route>} sinkToRouteMap
   * @return {boolean} |true| if the subtext should be hidden.
   * @private
   */
  computeSinkSubtextHidden_: function(sink, sinkToRouteMap) {
    if (!this.isEmptyOrWhitespace_(sink.description))
      return false;

    var route = sinkToRouteMap[sink.id];
    return !route || this.isEmptyOrWhitespace_(route.description);
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
    return this.castModeList.find(function(element, index, array) {
      return element.type == castModeType;
    });
  },

  /**
   * Returns whether given string is undefined, null, empty, or whitespace only.
   * @param {?string} str String to be tested.
   * @return {boolean} |true| if the string is undefined, null, empty, or
   *     whitespace.
   * @private
   */
  isEmptyOrWhitespace_: function(str) {
    return str === undefined || str === null || (/^\s*$/).test(str);
  },

  /**
   * Updates |currentView_| if the dialog had just opened and there's
   * only one local route.
   */
  maybeShowRouteDetailsOnOpen: function() {
    var localRoute = null;
    for (var i = 0; i < this.routeList.length; i++) {
      var route = this.routeList[i];
      if (!route.isLocal)
        continue;
      if (!localRoute) {
        localRoute = route;
      } else {
        // Don't show route details if there are more than one local route.
        localRoute = null;
        break;
      }
    }

    if (localRoute)
      this.showRouteDetails_(localRoute);
    this.fire('show-initial-state', {currentView: this.currentView_});
  },

  /**
   * Updates |currentView_| if there is a new blocking issue.
   *
   * @param {?media_router.Issue} issue The new issue.
   * @private
   */
  maybeShowIssueView_: function(issue) {
    if (!!issue && issue.isBlocking) {
      this.currentView_ = media_router.MediaRouterView.ISSUE;
    } else {
      this.async(function() {
        this.updateElementPositioning_();
      });
    }
  },

  /**
   * May update |populatedSinkListSeenTimeMs_| depending on |currentView| and
   * |sinksToShow|.
   * Called when |currentView_| or |sinksToShow_| is updated.
   *
   * @param {?media_router.MediaRouterView} currentView The current view of the
   *                                        dialog.
   * @param {!Array<!media_router.Sink>} sinksToShow The sinks to display.
   * @private
   */
  maybeUpdateStartSinkDisplayStartTime_: function(currentView, sinksToShow) {
    if (currentView == media_router.MediaRouterView.SINK_LIST &&
        sinksToShow.length != 0) {
      // Only set |populatedSinkListSeenTimeMs_| if it has not already been set.
      if (this.populatedSinkListSeenTimeMs_ == -1)
        this.populatedSinkListSeenTimeMs_ = performance.now();
    } else {
      // Reset |populatedSinkListLastSeen_| if the sink list isn't being shown
      // or if there aren't any sinks available for display.
      this.populatedSinkListSeenTimeMs_ = -1;
    }
  },

  /**
   * Handles a cast mode selection. Updates |headerText|, |headerTextTooltip|,
   * and |shownCastModeValue_|.
   *
   * @param {!Event} event The event object.
   * @private
   */
  onCastModeClick_: function(event) {
    // The clicked cast mode can come from one of two lists,
    // defaultCastModeList and nonDefaultCastModeList.
    var clickedMode =
        this.$$('#defaultCastModeList').itemForElement(event.target) ||
            this.$$('#nonDefaultCastModeList').itemForElement(event.target);

    if (!clickedMode)
      return;

    this.userHasSelectedCastMode_ = true;
    this.fire('cast-mode-selected', {castModeType: clickedMode.type});

    // The list of sinks to show will be the same if the shown cast mode did
    // not change, regardless of whether the user selected it explicitly.
    if (clickedMode.type != this.shownCastModeValue_) {
      this.setShownCastMode_(clickedMode);
      this.rebuildSinksToShow_();
    }

    this.showSinkList_();
    this.maybeReportUserFirstAction(
        media_router.MediaRouterUserAction.CHANGE_MODE);
  },

  /**
   * Handles a close-route-click event. Shows the sink list and starts a timer
   * to close the dialog if there is no click within three seconds.
   *
   * @param {!Event} event The event object.
   * Parameters in |event|.detail:
   *   route - route to close.
   * @private
   */
  onCloseRouteClick_: function(event) {
    /** @type {{route: media_router.Route}} */
    var detail = event.detail;
    this.showSinkList_();
    this.startTapTimer_();

    if (detail.route.isLocal) {
      this.maybeReportUserFirstAction(
          media_router.MediaRouterUserAction.STOP_LOCAL);
    }
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

    // Check that |sinkId| exists.
    if (!this.sinkMap_[sinkId])
      return;

    // If there is an existing route associated with the same sink, its
    // |sinkToRouteMap_| entry will be overwritten with that of the new route,
    // which results in the correct sink to route mapping.
    this.routeList.push(route);
    this.showRouteDetails_(route);

    this.startTapTimer_();
  },

  /**
   * Called when a mouseleave event is triggered.
   *
   * @private
   */
  onMouseLeave_: function() {
    this.mouseIsPositionedOverDialog_ = false;
  },

  /**
   * Called when a mouseenter event is triggered.
   *
   * @private
   */
  onMouseEnter_: function() {
    this.mouseIsPositionedOverDialog_ = true;
  },

  /**
   * Handles timeout of previous create route attempt. Clearing
   * |currentLaunchingSinkId_| hides the spinner indicating there is a route
   * creation in progress and show the device icon instead.
   */
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
    this.fire('sink-click', {index: event['model'].index});
  },

  /**
   * Called when |routeList| is updated. Rebuilds |routeMap_| and
   * |sinkToRouteMap_|.
   *
   * @private
   */
  rebuildRouteMaps_: function() {
    this.routeMap_ = {};

    // Rebuild |sinkToRouteMap_| with a temporary map to avoid firing the
    // computed functions prematurely.
    var tempSinkToRouteMap = {};

    // We expect that each route in |routeList| maps to a unique sink.
    this.routeList.forEach(function(route) {
      this.routeMap_[route.id] = route;
      tempSinkToRouteMap[route.sinkId] = route;
    }, this);

    // If |currentRoute_| is no longer active, clear |currentRoute_|. Also
    // switch back to the SINK_PICKER view if the user is currently in the
    // ROUTE_DETAILS view.
    if (!this.currentRoute_ || !this.routeMap_[this.currentRoute_.id]) {
      if (this.currentView_ == media_router.MediaRouterView.ROUTE_DETAILS) {
        // We may have an updated route to show for a device.
        // We swap out |currentRoute_| (and consequently the route-details
        // controls) to handle this.
        this.currentRoute_ =
            tempSinkToRouteMap[this.currentRoute_.sinkId] || null;

        if (!this.currentRoute_)
          this.showSinkList_();
      } else {
        this.currentRoute_ = null;
      }
    }

    this.sinkToRouteMap_ = tempSinkToRouteMap;
    this.rebuildSinksToShow_();
  },

  /**
   * Rebuilds the list of sinks to be shown for the current cast mode.
   * A sink should be shown if it is compatible with the current cast mode, or
   * if the sink is associated with a route.  The resulting list is sorted by
   * name.
   */
  rebuildSinksToShow_: function() {
    var sinksToShow = [];
    if (this.userHasSelectedCastMode_) {
      // If user explicitly selected a cast mode, then we show only sinks that
      // are compatible with current cast mode or sinks that are active.
      sinksToShow = this.allSinks.filter(function(element) {
        return (element.castModes & this.shownCastModeValue_) ||
               this.sinkToRouteMap_[element.id];
      }, this);
    } else {
      // If user did not select a cast mode, then:
      // - If all sinks support only a single cast mode, then the cast mode is
      //   switched to that mode.
      // - Otherwise, the cast mode becomes auto mode.
      // Either way, all sinks will be shown.
      this.setShownCastMode_(this.computeCastMode_());
      sinksToShow = this.allSinks;
    }

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
   * Updates the shown cast mode, and updates the header text fields
   * according to the cast mode. If |castMode| type is AUTO, then set
   * |userHasSelectedCastMode_| to false.
   *
   * @param {!media_router.CastMode} castMode
   */
  setShownCastMode_: function(castMode) {
    if (this.shownCastModeValue_ == castMode.type)
      return;

    this.shownCastModeValue_ = castMode.type;
    this.headerText = castMode.description;
    this.headerTextTooltip = castMode.host;
    if (castMode.type == media_router.CastModeType.AUTO)
      this.userHasSelectedCastMode_ = false;
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
   * Updates the top margins of the header and sink list view depending on
   * whether the first run flow is being shown.
   *
   * @param {boolean} showFirstRunFlow Whether or not to show the first run
   *     flow.
   * @param {!media_router.MediaRouterView} currentView The current view.
   * @private
   */
  shownComponentsChanged_: function(showFirstRunFlow, currentView) {
    var headerHeight = this.$$('#container-header').offsetHeight;
    if (this.computeShowFirstRunFlow_(showFirstRunFlow, currentView)) {
      // Ensures that first run flow elements have finished stamping.
      this.async(function() {
        var firstRunFlowHeight = this.$$('#first-run-flow') ?
            this.$$('#first-run-flow').offsetHeight : 0;
        this.$['container-header'].style.marginTop = firstRunFlowHeight + 'px';
        this.$['sink-list-view'].style.marginTop =
            firstRunFlowHeight + headerHeight + 'px';
      });
    } else {
      this.$['container-header'].style.marginTop = '0px';
      this.$['sink-list-view'].style.marginTop = headerHeight + 'px';
    }
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
      this.fire('navigate-sink-list-to-details');
      this.maybeReportUserFirstAction(
          media_router.MediaRouterUserAction.STATUS_REMOTE);
    } else if (this.currentLaunchingSinkId_ == '') {
      // Allow one launch at a time.
      this.fire('create-route', {
        sinkId: sink.id,
        // If user selected a cast mode, then we will create a route using that
        // cast mode. Otherwise, the UI is in "auto" cast mode and will use the
        // preferred cast mode compatible with the sink. The preferred cast mode
        // value is the least significant bit on the bitset.
        selectedCastModeValue:
            this.shownCastModeValue_ == media_router.CastModeType.AUTO ?
                sink.castModes & -sink.castModes : this.shownCastModeValue_
      });
      this.currentLaunchingSinkId_ = sink.id;

      var timeToSelectSink =
          performance.now() - this.populatedSinkListSeenTimeMs_;
      this.fire('report-sink-click-time', {timeMs: timeToSelectSink});

      this.maybeReportUserFirstAction(
          media_router.MediaRouterUserAction.START_LOCAL);
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
   * Starts a timer which fires a close-dialog event if the user's mouse is
   * not positioned over the dialog after three seconds.
   *
   * @private
   */
  startTapTimer_: function() {
    var id = setTimeout(function() {
      if (!this.mouseIsPositionedOverDialog_)
        this.fire('close-dialog');
    }.bind(this), 3000 /* 3 seconds */);
  },

  /**
   * Toggles |currentView_| between CAST_MODE_LIST and SINK_LIST.
   *
   * @private
   */
  toggleCastModeHidden_: function() {
    if (this.currentView_ == media_router.MediaRouterView.CAST_MODE_LIST) {
      this.showSinkList_();
    } else {
      this.showCastModeList_();
      this.fire('navigate-to-cast-mode-list');
    }
  },

  /**
   * Update the position-related styling of some elements.
   *
   * @private
   */
  updateElementPositioning_: function() {
    var headerHeight = this.$$('#container-header').offsetHeight;
    var firstRunFlowHeight = this.$$('#first-run-flow') ?
        this.$$('#first-run-flow').offsetHeight : 0;
    var issueHeight = this.$$('#issue-banner') ?
        this.$$('#issue-banner').offsetHeight : 0;

    this.$['container-header'].style.marginTop = firstRunFlowHeight + 'px';
    this.$['sink-list-view'].style.marginTop =
        firstRunFlowHeight + headerHeight + 'px';
    this.$['sink-list'].style.maxHeight =
        this.dialogHeight_ - headerHeight - firstRunFlowHeight -
            issueHeight + 'px';
  },

  /**
   * Update the max dialog height and update the positioning of the elements.
   *
   * @param {number} height The max height of the Media Router dialog.
   */
  updateMaxDialogHeight: function(height) {
    this.dialogHeight_ = height;
    this.updateElementPositioning_();
  },
});
