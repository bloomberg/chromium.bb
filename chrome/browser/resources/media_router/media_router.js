// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="media_router_data.js">
<include src="media_router_ui_interface.js">

// Handles user events for the Media Router UI.
cr.define('media_router', function() {
  'use strict';

  // The ESC key maps to keycode '27'.
  // @const {number}
  var KEYCODE_ESC = 27;

  // The media-router-container element. Initialized after polymer is ready.
  var container = null;

  /**
   * Initializes the Media Router WebUI and requests initial media
   * router content, such as the media sink and media route lists.
   */
  function initialize() {
    media_router.browserApi.requestInitialData();

    container = $('media-router-container');
    media_router.ui.setContainer(container);

    container.addEventListener('acknowledge-first-run-flow',
                               onAcknowledgeFirstRunFlow);
    container.addEventListener('back-click', onNavigateToSinkList);
    container.addEventListener('cast-mode-selected', onCastModeSelected);
    container.addEventListener('close-button-click', onCloseDialogEvent);
    container.addEventListener('close-dialog', onCloseDialogEvent);
    container.addEventListener('close-route-click', onCloseRouteClick);
    container.addEventListener('create-route', onCreateRoute);
    container.addEventListener('issue-action-click', onIssueActionClick);
    container.addEventListener('join-route-click', onJoinRouteClick);
    container.addEventListener('navigate-sink-list-to-details',
                               onNavigateToDetails);
    container.addEventListener('navigate-to-cast-mode-list',
                               onNavigateToCastMode);
    container.addEventListener('report-initial-action', onInitialAction);
    container.addEventListener('report-initial-action-close',
                               onInitialActionClose);
    container.addEventListener('report-sink-click-time',
                               onSinkClickTimeReported);
    container.addEventListener('report-sink-count', onSinkCountReported);
    container.addEventListener('show-initial-state', onShowInitialState);
    container.addEventListener('sink-click', onSinkClick);

    // Pressing the ESC key closes the dialog.
    document.addEventListener('keydown', function(e) {
      if (e.keyCode == KEYCODE_ESC) {
        container.maybeReportUserFirstAction(
            media_router.MediaRouterUserAction.CLOSE);
      }
    });
  }

  /**
   * Reports the selected cast mode.
   * Called when the user selects a cast mode from the picker.
   *
   * @param {{detail: {castModeType: number}}} data
   * Parameters in |data|.detail:
   *   castModeType - type of cast mode selected by the user.
   */
  function onCastModeSelected(data) {
    media_router.browserApi.reportSelectedCastMode(data.detail.castModeType);
  }

  /**
   * Updates the preference that the user has seen the first run flow.
   * Called when the user clicks on the acknowledgement button on the first run
   * flow.
   */
  function onAcknowledgeFirstRunFlow() {
    media_router.browserApi.acknowledgeFirstRunFlow();
  }

  /**
   * Closes the dialog.
   * Called when the user clicks the close button on the dialog.
   */
  function onCloseDialogEvent() {
    container.maybeReportUserFirstAction(
        media_router.MediaRouterUserAction.CLOSE);
    media_router.browserApi.closeDialog();
  }

  /**
   * Reports the first action the user takes after opening the dialog.
   * Called when the user explicitly interacts with the dialog to perform an
   * action.
   *
   * @param {{detail: {action: number}}} data
   * Parameters in |data|.detail:
   *   action - the first action taken by the user.
   */
  function onInitialAction(data) {
    media_router.browserApi.reportInitialAction(data.detail.action);
  }

  /**
   * Reports the time it took for the user to close the dialog if that was the
   * first action the user took after opening the dialog.
   * Called when the user closes the dialog without taking any other action.
   *
   * @param {{detail: {timeMs: number}}} data
   * Parameters in |data|.detail:
   *   timeMs - time in ms for the user to close the dialog.
   */
  function onInitialActionClose(data) {
    media_router.browserApi.reportTimeToInitialActionClose(data.detail.timeMs);
  }

  /**
   * Acts on an issue and dismisses it from the UI.
   * Called when the user performs an action on an issue.
   *
   * @param {{detail: {id: string, actionType: number, helpPageId: number}}}
   *     data
   * Parameters in |data|.detail:
   *   id - issue ID.
   *   actionType - type of action performed by the user.
   *   helpPageId - the numeric help center ID.
   */
  function onIssueActionClick(data) {
    media_router.browserApi.actOnIssue(data.detail.id,
                                       data.detail.actionType,
                                       data.detail.helpPageId);
    container.issue = null;
  }

  /**
   * Creates a media route.
   * Called when the user requests to create a media route.
   *
   * @param {{detail: {sinkId: string, selectedCastModeValue: number}}} data
   * Parameters in |data|.detail:
   *   sinkId - sink ID selected by the user.
   *   selectedCastModeValue - cast mode selected by the user.
   */
  function onCreateRoute(data) {
    media_router.browserApi.requestRoute(data.detail.sinkId,
                                         data.detail.selectedCastModeValue);
  }

  /**
   * Stops a route.
   * Called when the user requests to stop a media route.
   *
   * @param {{detail: {route: media_router.Route}}} data
   * Parameters in |data|.detail:
   *   route - route to close.
   */
  function onCloseRouteClick(data) {
    media_router.browserApi.closeRoute(data.detail.route);
  }

  /**
   * Joins a route.
   * Called when the user requests to join a media route.
   *
   * @param {{detail: {route: media_router.Route}}} data
   * Parameters in |data|.detail:
   *   route - route to join.
   */
  function onJoinRouteClick(data) {
    media_router.browserApi.joinRoute(data.detail.route);
  }

  /**
   * Reports the index of the sink that was clicked.
   * Called when the user selects a sink on the sink list.
   *
   * @param {{detail: {index: number}}} data
   * Paramters in |data|.detail:
   *   index - the index of the clicked sink.
   */
  function onSinkClick(data) {
    media_router.browserApi.reportClickedSinkIndex(data.detail.index);
  }

  /**
   * Reports the user navigation to the cast mode view.
   * Called when the user clicks the drop arrow to navigate to the cast mode
   * view on the dialog.
   */
  function onNavigateToCastMode() {
    media_router.browserApi.reportNavigateToView(
        media_router.MediaRouterView.CAST_MODE_LIST);
  }

  /**
   * Reports the user navigation the route details view.
   * Called when the user clicks on a sink to navigate to the route details
   * view.
   */
  function onNavigateToDetails() {
    media_router.browserApi.reportNavigateToView(
        media_router.MediaRouterView.ROUTE_DETAILS);
  }

  /**
   * Reports the user navigation the sink list view.
   * Called when the user clicks on the back button from the route details view
   * to the sink list view.
   */
  function onNavigateToSinkList() {
    media_router.browserApi.reportNavigateToView(
        media_router.MediaRouterView.SINK_LIST);
  }

  /*
   * Reports the initial state of the dialog after it is opened.
   * Called after initial data is populated.
   *
   * @param {{detail: {currentView: string}}} data
   * Parameters in |data|.detail:
   *   currentView - the current dialog's current view.
   */
  function onShowInitialState(data) {
    media_router.browserApi.reportInitialState(data.detail.currentView);
  }

  /**
   * Reports the index of the sink that was clicked.
   * Called when the user selects a sink on the sink list.
   *
   * @param {{detail: {index: number}}} data
   * Paramters in |data|.detail:
   *   index - the index of the clicked sink.
   */
  function onSinkClick(data) {
    media_router.browserApi.reportClickedSinkIndex(data.detail.index);
  }

  /**
   * Reports the time it took for the user to select a sink to create a route
   * after the list was popuated and shown.
   *
   * @param {{detail: {timeMs: number}}} data
   * Paramters in |data|.detail:
   *   timeMs - the time it took for the user to select a sink.
   */
  function onSinkClickTimeReported(data) {
    media_router.browserApi.reportTimeToClickSink(data.detail.timeMs);
  }

  /**
   * Reports the current sink count.
   * Called 3 seconds after the dialog is initially opened.
   *
   * @param {{detail: {sinkCount: number}}} data
   * Parameters in |data|.detail:
   *   sinkCount - the number of sinks.
   */
  function onSinkCountReported(data) {
    media_router.browserApi.reportSinkCount(data.detail.sinkCount);
  }

  return {
    initialize: initialize,
  };
});

window.addEventListener('load', media_router.initialize);
