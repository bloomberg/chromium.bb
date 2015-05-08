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
   * Adds a new route.
   *
   * @param {!media_router.Route} route
   */
  function addRoute(route) {
    container.addRoute(route);
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
   * Sets the current issue.
   *
   * @param {!media_router.Issue} issue
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
    container.sinkList = sinkList;
  }

  return {
    addRoute: addRoute,
    setCastModeList: setCastModeList,
    setContainer: setContainer,
    setIssue: setIssue,
    setRouteList: setRouteList,
    setSinkList: setSinkList,
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
   * @param {string} sinkId The sink ID.
   * @param {number} selectedCastMode The value of the cast mode the user
   *   selected, or -1 if the user has not explicitly selected a mode.
   */
  function requestRoute(sinkId, selectedCastMode) {
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
