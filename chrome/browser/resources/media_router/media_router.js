// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="media_router_data.js">
<include src="media_router_ui_interface.js">

// Handles user events for the Media Router UI.
cr.define('media_router', function() {
  'use strict';

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

    container.addEventListener('back-click', onNavigateToSinkList);
    container.addEventListener('cast-mode-selected', onCastModeSelected);
    container.addEventListener('close-button-click', onCloseDialogEvent);
    container.addEventListener('close-dialog', onCloseDialogEvent);
    container.addEventListener('close-route-click', onCloseRouteClick);
    container.addEventListener('create-route', onCreateRoute);
    container.addEventListener('issue-action-click', onIssueActionClick);
    container.addEventListener('navigate-sink-list-to-details',
                               onNavigateToDetails);
    container.addEventListener('navigate-to-cast-mode-list',
                               onNavigateToCastMode);
    container.addEventListener('report-sink-count', onSinkCountReported);
    container.addEventListener('sink-click', onSinkClick);
  }

  /**
   * Closes the dialog.
   * Called when the user clicks the close button on the dialog.
   */
  function onCloseDialogEvent() {
    media_router.browserApi.closeDialog();
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
   * @param {{detail: {route: string}}} data
   * Parameters in |data|.detail:
   *   route - route ID.
   */
  function onCloseRouteClick(data) {
    media_router.browserApi.closeRoute(data.detail.route);
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
