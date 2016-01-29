// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// API invoked by the browser MediaRouterWebUIMessageHandler to communicate
// with this UI.
cr.define('media_router.ui', function() {
  'use strict';

  // The media-router-container element.
  var container = null;

  /**
   * Handles timeout of previous create route attempt.
   */
  function onNotifyRouteCreationTimeout() {
    container.onNotifyRouteCreationTimeout();
  }

  /**
   * Handles response of previous create route attempt.
   *
   * @param {string} sinkId The ID of the sink to which the Media Route was
   *     creating a route.
   * @param {string} routeId The ID of the newly created route that corresponds
   *     to the sink if route creation succeeded; empty otherwise.
   */
  function onCreateRouteResponseReceived(sinkId, routeId) {
    container.onCreateRouteResponseReceived(sinkId, routeId);
  }

  /**
   * Sets the cast mode list.
   *
   * @param {!Array<!media_router.CastMode>} castModeList
   */
  function setCastModeList(castModeList) {
    container.castModeList = castModeList;
  }

  /**
   * Sets |container|.
   *
   * @param {!MediaRouterContainerElement} mediaRouterContainer
   */
  function setContainer(mediaRouterContainer) {
    container = mediaRouterContainer;
  }

  /**
   * Populates the WebUI with data obtained from Media Router.
   *
   * @param {{firstRunFlowCloudPrefLearnMoreUrl: string,
   *          deviceMissingUrl: string,
   *          sinks: !Array<!media_router.Sink>,
   *          routes: !Array<!media_router.Route>,
   *          castModes: !Array<!media_router.CastMode>,
   *          wasFirstRunFlowAcknowledged: boolean,
   *          showFirstRunFlowCloudPref: boolean}} data
   * Parameters in data:
   *   firstRunFlowCloudPrefLearnMoreUrl - url to open when the cloud services
   *       pref learn more link is clicked.
   *   deviceMissingUrl - url to be opened on "Device missing?" clicked.
   *   sinks - list of sinks to be displayed.
   *   routes - list of routes that are associated with the sinks.
   *   castModes - list of available cast modes.
   *   wasFirstRunFlowAcknowledged - true if first run flow was previously
   *       acknowledged by user.
   *   showFirstRunFlowCloudPref - true if the cloud pref option should be
   *       shown.
   */
  function setInitialData(data) {
    container.firstRunFlowCloudPrefLearnMoreUrl =
        data['firstRunFlowCloudPrefLearnMoreUrl'];
    container.deviceMissingUrl = data['deviceMissingUrl'];
    container.castModeList = data['castModes'];
    container.allSinks = data['sinks'];
    container.routeList = data['routes'];
    container.showFirstRunFlowCloudPref =
        data['showFirstRunFlowCloudPref'];
    // Some users acknowledged the first run flow before the cloud prefs
    // setting was implemented. These users will see the first run flow
    // again.
    container.showFirstRunFlow = !data['wasFirstRunFlowAcknowledged'] ||
        container.showFirstRunFlowCloudPref;
    container.maybeShowRouteDetailsOnOpen();
    media_router.browserApi.onInitialDataReceived();
  }

  /**
   * Sets current issue to |issue|, or clears the current issue if |issue| is
   * null.
   *
   * @param {?media_router.Issue} issue
   */
  function setIssue(issue) {
    container.issue = issue;
  }

  /**
   * Sets the list of currently active routes.
   *
   * @param {!Array<!media_router.Route>} routeList
   */
  function setRouteList(routeList) {
    container.routeList = routeList;
  }

  /**
   * Sets the list of discovered sinks.
   *
   * @param {!Array<!media_router.Sink>} sinkList
   */
  function setSinkList(sinkList) {
    container.allSinks = sinkList;
  }

  /**
   * Updates the max height of the dialog
   *
   * @param {number} height
   */
  function updateMaxHeight(height) {
    container.updateMaxDialogHeight(height);
  }

  return {
    onNotifyRouteCreationTimeout: onNotifyRouteCreationTimeout,
    onCreateRouteResponseReceived: onCreateRouteResponseReceived,
    setCastModeList: setCastModeList,
    setContainer: setContainer,
    setInitialData: setInitialData,
    setIssue: setIssue,
    setRouteList: setRouteList,
    setSinkList: setSinkList,
    updateMaxHeight: updateMaxHeight,
  };
});

// API invoked by this UI to communicate with the browser WebUI message handler.
cr.define('media_router.browserApi', function() {
  'use strict';

  /**
   * Indicates that the user has acknowledged the first run flow.
   *
   * @param {boolean} optedIntoCloudServices Whether or not the user opted into
   *                  cloud services.
   */
  function acknowledgeFirstRunFlow(optedIntoCloudServices) {
    chrome.send('acknowledgeFirstRunFlow', [optedIntoCloudServices]);
  }

  /**
   * Acts on the given issue.
   *
   * @param {string} issueId
   * @param {number} actionType Type of action that the user clicked.
   * @param {?number} helpPageId The numeric help center ID.
   */
  function actOnIssue(issueId, actionType, helpPageId) {
    chrome.send('actOnIssue', [{issueId: issueId, actionType: actionType,
                                helpPageId: helpPageId}]);
  }

  /**
   * Closes the dialog.
   */
  function closeDialog() {
    chrome.send('closeDialog');
  }

  /**
   * Closes the given route.
   *
   * @param {!media_router.Route} route
   */
  function closeRoute(route) {
    chrome.send('closeRoute', [{routeId: route.id, isLocal: route.isLocal}]);
  }

  /**
   * Joins the given route.
   *
   * @param {!media_router.Route} route
   */
  function joinRoute(route) {
    chrome.send('joinRoute', [{sinkId: route.sinkId, routeId: route.id}]);
  }

  /**
   * Indicates that the initial data has been received.
   */
  function onInitialDataReceived() {
    chrome.send('onInitialDataReceived');
  }

  /**
   * Reports the index of the selected sink.
   *
   * @param {number} sinkIndex
   */
  function reportClickedSinkIndex(sinkIndex) {
    chrome.send('reportClickedSinkIndex', [sinkIndex]);
  }

  /**
   * Reports the initial dialog view.
   *
   * @param {string} view
   */
  function reportInitialState(view) {
    chrome.send('reportInitialState', [view]);
  }

  /**
   * Reports the initial action the user took.
   *
   * @param {number} action
   */
  function reportInitialAction(action) {
    chrome.send('reportInitialAction', [action]);
  }

  /**
   * Reports the navigation to the specified view.
   *
   * @param {string} view
   */
  function reportNavigateToView(view) {
    chrome.send('reportNavigateToView', [view]);
  }

  /**
   * Reports whether or not a route was created successfully.
   *
   * @param {boolean} success
   */
  function reportRouteCreation(success) {
    chrome.send('reportRouteCreation', [success]);
  }

  /**
   * Reports the cast mode that the user selected.
   *
   * @param {number} castModeType
   */
  function reportSelectedCastMode(castModeType) {
    chrome.send('reportSelectedCastMode', [castModeType]);
  }

  /**
   * Reports the current number of sinks.
   *
   * @param {number} sinkCount
   */
  function reportSinkCount(sinkCount) {
    chrome.send('reportSinkCount', [sinkCount]);
  }

  /**
   * Reports the time it took for the user to select a sink after the sink list
   * is populated and shown.
   *
   * @param {number} timeMs
   */
  function reportTimeToClickSink(timeMs) {
    chrome.send('reportTimeToClickSink', [timeMs]);
  }

  /**
   * Reports the time, in ms, it took for the user to close the dialog without
   * taking any other action.
   *
   * @param {number} timeMs
   */
  function reportTimeToInitialActionClose(timeMs) {
    chrome.send('reportTimeToInitialActionClose', [timeMs]);
  }

  /**
   * Requests data to initialize the WebUI with.
   * The data will be returned via media_router.ui.setInitialData.
   */
  function requestInitialData() {
    chrome.send('requestInitialData');
  }

  /**
   * Requests that a media route be started with the given sink.
   *
   * @param {string} sinkId The sink ID.
   * @param {number} selectedCastMode The value of the cast mode the user
   *   selected.
   */
  function requestRoute(sinkId, selectedCastMode) {
    chrome.send('requestRoute',
                [{sinkId: sinkId, selectedCastMode: selectedCastMode}]);
  }

  return {
    acknowledgeFirstRunFlow: acknowledgeFirstRunFlow,
    actOnIssue: actOnIssue,
    closeDialog: closeDialog,
    closeRoute: closeRoute,
    joinRoute: joinRoute,
    onInitialDataReceived: onInitialDataReceived,
    reportClickedSinkIndex: reportClickedSinkIndex,
    reportInitialAction: reportInitialAction,
    reportInitialState: reportInitialState,
    reportNavigateToView: reportNavigateToView,
    reportSelectedCastMode: reportSelectedCastMode,
    reportRouteCreation: reportRouteCreation,
    reportSinkCount: reportSinkCount,
    reportTimeToClickSink: reportTimeToClickSink,
    reportTimeToInitialActionClose: reportTimeToInitialActionClose,
    requestInitialData: requestInitialData,
    requestRoute: requestRoute,
  };
});
