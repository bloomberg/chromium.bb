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
    // TODO(apacible): Add chrome.send call when browser WebUI message
    // handler is implemented.

    container = $('media-router-container');
    media_router.ui.setContainer(container);

    container.addEventListener('cast-mode-click', onCastModeClick);
    container.addEventListener('close-button-click', onCloseDialogClick);
    container.addEventListener('close-route-click', onCloseRouteClick);
    container.addEventListener('create-route', onCreateRoute);
    container.addEventListener('issue-action-click', onIssueActionClick);
  }

  /**
   * Changes the UI, such as the the header text, in response to a cast mode
   * change.
   * Called when the user selects a cast mode.
   *
   * @param {{detail: {headerText: string}}} data
   * Parameters in |data|.detail:
   *   headerText - the new header text corresponding to the selected
   *                cast mode.
   */
  function onCastModeClick(data) {
    container.headerText = data.detail.headerText;
  }

  /**
   * Closes the dialog.
   * Called when the user clicks the close button on the dialog.
   */
  function onCloseDialogClick() {
    media_router.browserApi.closeDialog();
  }

  /**
   * Acts on an issue and dismisses it from the UI.
   * Called when the user performs an action on an issue.
   *
   * @param {{detail: {id: string, actionType: number, helpURL: string}}} data
   * Parameters in |data|.detail:
   *   id - issue ID.
   *   actionType - type of action performed by the user.
   *   helpURL - the help URL for the issue.
   */
  function onIssueActionClick(data) {
    media_router.browserApi.actOnIssue(data.detail.id,
                                       data.detail.actionType,
                                       data.detail.helpURL);
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

window.addEventListener('polymer-ready', media_router.initialize);
