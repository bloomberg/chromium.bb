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

  /**
   * The media-router-container element. Initialized after polymer is ready.
   * @type {?MediaRouterContainerElement}
   */
  var container = null;

  /**
   * Initializes the Media Router WebUI and requests initial media
   * router content, such as the media sink and media route lists.
   */
  function initialize() {
    media_router.browserApi.requestInitialData();

    container = /** @type {!MediaRouterContainerElement} */
        ($('media-router-container'));
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
    container.addEventListener('report-route-creation', onReportRouteCreation);
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
   * @param {!Event} event
   * Parameters in |event|.detail:
   *   castModeType - type of cast mode selected by the user.
   */
  function onCastModeSelected(event) {
    /** @type {{castModeType: number}} */
    var detail = event.detail;
    media_router.browserApi.reportSelectedCastMode(detail.castModeType);
  }

  /**
   * Updates the preference that the user has seen the first run flow.
   * Called when the user clicks on the acknowledgement button on the first run
   * flow.
   *
   * @param {!Event} event
   * Parameters in |event|.detail:
   *   optedIntoCloudServices - whether or not the user opted into cloud
   *                            services.
   */
  function onAcknowledgeFirstRunFlow(event) {
    /** @type {{optedIntoCloudServices: boolean}} */
    var detail = event.detail;
    media_router.browserApi.acknowledgeFirstRunFlow(
        detail.optedIntoCloudServices);
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
   * @param {!Event} event
   * Parameters in |event|.detail:
   *   action - the first action taken by the user.
   */
  function onInitialAction(event) {
    /** @type {{action: number}} */
    var detail = event.detail;
    media_router.browserApi.reportInitialAction(detail.action);
  }

  /**
   * Reports the time it took for the user to close the dialog if that was the
   * first action the user took after opening the dialog.
   * Called when the user closes the dialog without taking any other action.
   *
   * @param {!Event} event
   * Parameters in |event|.detail:
   *   timeMs - time in ms for the user to close the dialog.
   */
  function onInitialActionClose(event) {
    /** @type {{timeMs: number}} */
    var detail = event.detail;
    media_router.browserApi.reportTimeToInitialActionClose(detail.timeMs);
  }

  /**
   * Acts on an issue and dismisses it from the UI.
   * Called when the user performs an action on an issue.
   *
   * @param {!Event} event
   * Parameters in |event|.detail:
   *   id - issue ID.
   *   actionType - type of action performed by the user.
   *   helpPageId - the numeric help center ID.
   */
  function onIssueActionClick(event) {
    /** @type {{id: string, actionType: number, helpPageId: number}} */
    var detail = event.detail;
    media_router.browserApi.actOnIssue(detail.id,
                                       detail.actionType,
                                       detail.helpPageId);
    container.issue = null;
  }

  /**
   * Creates a media route.
   * Called when the user requests to create a media route.
   *
   * @param {!Event} event
   * Parameters in |event|.detail:
   *   sinkId - sink ID selected by the user.
   *   selectedCastModeValue - cast mode selected by the user.
   */
  function onCreateRoute(event) {
    /** @type {{sinkId: string, selectedCastModeValue, number}} */
    var detail = event.detail;
    media_router.browserApi.requestRoute(detail.sinkId,
                                         detail.selectedCastModeValue);
  }

  /**
   * Stops a route.
   * Called when the user requests to stop a media route.
   *
   * @param {!Event} event
   * Parameters in |event|.detail:
   *   route - The route to close.
   */
  function onCloseRouteClick(event) {
    /** @type {{route: !media_router.Route}} */
    var detail = event.detail;
    media_router.browserApi.closeRoute(detail.route);
  }

  /**
   * Joins a route.
   * Called when the user requests to join a media route.
   *
   * @param {!Event} event
   * Parameters in |event|.detail:
   *   route - route to join.
   */
  function onJoinRouteClick(event) {
    /** @type {{route: !media_router.Route}} */
    var detail = event.detail;
    media_router.browserApi.joinRoute(detail.route);
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

  /**
   * Reports whether or not the route creation was successful.
   *
   * @param {!Event} event
   * Parameters in |event|.detail:
   *   success - whether or not the route creation was successful.
   */
  function onReportRouteCreation(event) {
    var detail = event.detail;
    media_router.browserApi.reportRouteCreation(detail.success);
  }

  /**
   * Reports the initial state of the dialog after it is opened.
   * Called after initial data is populated.
   *
   * @param {!Event} event
   * Parameters in |event|.detail:
   *   currentView - the current dialog's current view.
   */
  function onShowInitialState(event) {
    /** @type {{currentView: string}} */
    var detail = event.detail;
    media_router.browserApi.reportInitialState(detail.currentView);
  }

  /**
   * Reports the index of the sink that was clicked.
   * Called when the user selects a sink on the sink list.
   *
   * @param {!Event} event
   * Paramters in |event|.detail:
   *   index - the index of the clicked sink.
   */
  function onSinkClick(event) {
    /** @type {{index: number}} */
    var detail = event.detail;
    media_router.browserApi.reportClickedSinkIndex(detail.index);
  }

  /**
   * Reports the time it took for the user to select a sink to create a route
   * after the list was popuated and shown.
   *
   * @param {!Event} event
   * Paramters in |event|.detail:
   *   timeMs - the time it took for the user to select a sink.
   */
  function onSinkClickTimeReported(event) {
    /** @type {{timeMs: number}} */
    var detail = event.detail;
    media_router.browserApi.reportTimeToClickSink(detail.timeMs);
  }

  /**
   * Reports the current sink count.
   * Called 3 seconds after the dialog is initially opened.
   *
   * @param {!Event} event
   * Parameters in |event|.detail:
   *   sinkCount - the number of sinks.
   */
  function onSinkCountReported(event) {
    /** @type {{sinkCount: number}} */
    var detail = event.detail;
    media_router.browserApi.reportSinkCount(detail.sinkCount);
  }

  return {
    initialize: initialize,
  };
});

window.addEventListener('load', media_router.initialize);
