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

    container.addEventListener('close-button-click', onCloseDialogEvent);
    container.addEventListener('close-dialog', onCloseDialogEvent);
    container.addEventListener('close-route-click', onCloseRouteClick);
    container.addEventListener('create-route', onCreateRoute);
    container.addEventListener('issue-action-click', onIssueActionClick);
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

  return {
    initialize: initialize,
  };
});

window.addEventListener('load', media_router.initialize);
