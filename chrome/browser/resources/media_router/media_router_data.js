// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Any strings used here will already be localized. Values such as
// CastMode.type or IDs will be defined elsewhere and determined later.
cr.define('media_router', function() {
  'use strict';

  /**
   * This corresponds to the C++ MediaCastMode.
   * @enum {number}
   */
  var CastModeType = {
    DEFAULT: 0,
    TAB_MIRROR: 1,
    DESKTOP_MIRROR: 2,
  };

  /**
   * The possible states of the Media Router dialog. Used to determine which
   * components to show.
   * @enum {string}
   */
  var MediaRouterView = {
    CAST_MODE_LIST: 'cast-mode-list',
    FILTER: 'filter',
    ISSUE: 'issue',
    ROUTE_DETAILS: 'route-details',
    SINK_LIST: 'sink-list',
  };

  /**
   * This corresponds to the C++ MediaSink IconType.
   * @enum {mumber}
   */
  var SinkIconType = {
    CAST: 0,
    CAST_AUDIO: 1,
    CAST_AUDIO_GROUP: 2,
    GENERIC: 2,
    HANGOUT: 3,
  };

  /**
   * @enum {string}
   */
  var SinkStatus = {
    IDLE: 'idle',
    ACTIVE: 'active',
    REQUEST_PENDING: 'request_pending'
  };


  /**
   * @param {media_router.CastModeType} type The type of cast mode.
   * @param {string} description The description of the cast mode.
   * @param {string} host The hostname of the site to cast.
   * @constructor
   * @struct
   */
  var CastMode = function(type, description, host) {
    /** @type {number} */
    this.type = type;

    /** @type {string} */
    this.description = description;

    /** @type {string} */
    this.host = host || null;
  };


  /**
   * @param {string} id The ID of this issue.
   * @param {string} title The issue title.
   * @param {string} message The issue message.
   * @param {number} defaultActionType The type of default action.
   * @param {?number} secondaryActionType The type of optional action.
   * @param {?string} mediaRouteId The route ID to which this issue
   *                  pertains. If not set, this is a global issue.
   * @param {boolean} isBlocking True if this issue blocks other UI.
   * @param {?number} helpPageId The numeric help center ID.
   * @constructor
   * @struct
   */
  var Issue = function(id, title, message, defaultActionType,
                       secondaryActionType, mediaRouteId, isBlocking,
                       helpPageId) {
    /** @type {string} */
    this.id = id;

    /** @type {string} */
    this.title = title;

    /** @type {string} */
    this.message = message;

    /** @type {number} */
    this.defaultActionType = defaultActionType;

    /** @type {?number} */
    this.secondaryActionType = secondaryActionType;

    /** @type {?string} */
    this.mediaRouteId = mediaRouteId;

    /** @type {boolean} */
    this.isBlocking = isBlocking;

    /** @type {?number} */
    this.helpPageId = helpPageId;
  };


  /**
   * @param {string} id The media route ID.
   * @param {string} sinkId The ID of the media sink running this route.
   * @param {string} description The short description of this route.
   * @param {?number} tabId The ID of the tab in which web app is running and
   *                  accessing the route.
   * @param {boolean} isLocal True if this is a locally created route.
   * @param {?string} customControllerPath non-empty if this route has custom
   *                  controller.
   * @constructor
   * @struct
   */
  var Route = function(id, sinkId, description, tabId, isLocal,
      customControllerPath) {
    /** @type {string} */
    this.id = id;

    /** @type {string} */
    this.sinkId = sinkId;

    /** @type {string} */
    this.description = description;

    /** @type {?number} */
    this.tabId = tabId;

    /** @type {boolean} */
    this.isLocal = isLocal;

    /** @type {?string} */
    this.customControllerPath = customControllerPath;
  };


  /**
   * @param {string} id The ID of the media sink.
   * @param {string} name The name of the sink.
   * @param {media_router.SinkIconType} iconType the type of icon for the sink.
   * @param {media_router.SinkStatus} status The readiness state of the sink.
   * @param {!Array<number>} castModes Cast modes compatible with the sink.
   * @constructor
   * @struct
   */
  var Sink = function(id, name, iconType, status, castModes) {
    /** @type {string} */
    this.id = id;

    /** @type {string} */
    this.name = name;

    /** @type {SinkIconType} */
    this.iconType = iconType;

    /** @type {media_router.SinkStatus} */
    this.status = status;

    /** @type {!Array<number>} */
    this.castModes = castModes;
  };


  /**
   * @param {number} tabId The current tab ID.
   * @param {string} domain The domain of the current tab.
   * @constructor
   * @struct
   */
  var TabInfo = function(tabId, domain) {
    /** @type {number} */
    this.tabId = tabId;

    /** @type {string} */
    this.domain = domain;
  };

  return {
    CastModeType: CastModeType,
    MediaRouterView: MediaRouterView,
    SinkIconType: SinkIconType,
    SinkStatus: SinkStatus,
    CastMode: CastMode,
    Issue: Issue,
    Route: Route,
    Sink: Sink,
    TabInfo: TabInfo,
  };
});
