// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// API invoked by the browser MediaRouterWebUIMessageHandler to communicate
// with this UI.
cr.define('media_router.ui', function() {
  'use strict';

  /**
   * Sets the list of discovered sinks.
   *
   * @param {!Array<!media_router.Sink>} sinkList
   */
  function setSinkList(sinkList) {
    // TODO(imcheng): Implement.
  }

  /**
   * Sets the list of currently active routes.
   *
   * @param {!Array<!media_router.Route>} routeList
   */
  function setRouteList(routeList) {
    // TODO(imcheng): Implement.
  }

  /**
   * Sets the cast mode list.
   *
   * @param {!Array<!media_router.CastMode>} castModeList
   */
  function setCastModeList(castModeList) {
    // TODO(imcheng): Implement.
  }

  /**
   * Sets the current issue.
   *
   * @param {!media_router.Issue} issue
   */
  function setIssue(issue) {
    // TODO(imcheng): Implement.
  }

  return {
    addRoute: addRoute,
    setCastModeList: setCastModeList,
    setSinkList: setSinkList,
    setRouteList: setRouteList,
    setIssue: setIssue,
  };
});

// API invoked by this UI to communicate with the browser WebUI message handler.
cr.define('media_router.browserApi', function() {
  'use strict';

  /**
   * Acts on the given issue.
   *
   * @param {string} issueId
   * @param {number} actionType Type of action that the user clicked.
   * @param {?string} helpURL URL to open if the action is to learn more.
   */
  function actOnIssue(issueId, actionType, helpURL) {
    // TODO(imcheng): Implement.
  }

  /**
   * Closes the given route.
   *
   * @param {!media_router.Route} route
   */
  function closeRoute(route) {
    // TODO(imcheng): Implement.
  }

  /**
   * Requests that a media route be started with the given sink.
   *
   * @param {!media_router.Sink} sink
   * @param {number} selectedCastMode The value of the cast mode the user
   *   selected, or -1 if the user has not explicitly selected a mode.
   */
  function requestRoute(sink, selectedCastMode) {
    // TODO(imcheng): Implement.
  }

  /**
   * Closes the dialog.
   */
  function closeDialog() {
    chrome.send('closeDialog');
  }

  return {
    actOnIssue: actOnIssue,
    closeRoute: closeRoute,
    requestRoute: requestRoute,
    closeDialog: closeDialog,
  };
});
