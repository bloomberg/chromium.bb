// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var mediaRouter;

define('media_router_bindings', [
    'chrome/common/media_router/mojo/media_controller.mojom',
    'chrome/common/media_router/mojo/media_router.mojom',
    'chrome/common/media_router/mojo/media_status.mojom',
    'content/public/renderer/frame_interfaces',
    'extensions/common/mojo/keep_alive.mojom',
    'media/mojo/interfaces/mirror_service_remoting.mojom',
    'media/mojo/interfaces/remoting_common.mojom',
    'mojo/common/time.mojom',
    'mojo/public/js/bindings',
    'net/interfaces/ip_address.mojom',
    'url/mojo/origin.mojom',
    'url/mojo/url.mojom',
], function(mediaControllerMojom,
            mediaRouterMojom,
            mediaStatusMojom,
            frameInterfaces,
            keepAliveMojom,
            remotingMojom,
            remotingCommonMojom,
            timeMojom,
            bindings,
            ipAddressMojom,
            originMojom,
            urlMojom) {
  'use strict';

  /**
   * Converts a media sink to a MediaSink Mojo object.
   * @param {!MediaSink} sink A media sink.
   * @return {!mediaRouterMojom.MediaSink} A Mojo MediaSink object.
   */
  function sinkToMojo_(sink) {
    return new mediaRouterMojom.MediaSink({
      'name': sink.friendlyName,
      'description': sink.description,
      'domain': sink.domain,
      'sink_id': sink.id,
      'icon_type': sinkIconTypeToMojo(sink.iconType),
    });
  }

  /**
   * Converts a media sink's icon type to a MediaSink.IconType Mojo object.
   * @param {!MediaSink.IconType} type A media sink's icon type.
   * @return {!mediaRouterMojom.MediaSink.IconType} A Mojo MediaSink.IconType
   *     object.
   */
  function sinkIconTypeToMojo(type) {
    switch (type) {
      case 'cast':
        return mediaRouterMojom.SinkIconType.CAST;
      case 'cast_audio_group':
        return mediaRouterMojom.SinkIconType.CAST_AUDIO_GROUP;
      case 'cast_audio':
        return mediaRouterMojom.SinkIconType.CAST_AUDIO;
      case 'meeting':
        return mediaRouterMojom.SinkIconType.MEETING;
      case 'hangout':
        return mediaRouterMojom.SinkIconType.HANGOUT;
      case 'education':
        return mediaRouterMojom.SinkIconType.EDUCATION;
      case 'generic':
        return mediaRouterMojom.SinkIconType.GENERIC;
      default:
        console.error('Unknown sink icon type : ' + type);
        return mediaRouterMojom.SinkIconType.GENERIC;
    }
  }

  /**
   * Returns a Mojo MediaRoute object given a MediaRoute and a
   * media sink name.
   * @param {!MediaRoute} route
   * @return {!mojo.MediaRoute}
   */
  function routeToMojo_(route) {
    return new mediaRouterMojom.MediaRoute({
      'media_route_id': route.id,
      'media_source': route.mediaSource,
      'media_sink_id': route.sinkId,
      'description': route.description,
      'icon_url': route.iconUrl,
      'is_local': route.isLocal,
      'custom_controller_path': route.customControllerPath,
      'for_display': route.forDisplay,
      'is_incognito': route.offTheRecord,
      'is_offscreen_presentation': route.isOffscreenPresentation,
      // Begin newly added properties, followed by the milestone they were
      // added.  The guard should be safe to remove N+2 milestones later.
      'supports_media_route_controller':
          !!route.supportsMediaRouteController  // M61
    });
  }

  /**
   * Converts a route message to a RouteMessage Mojo object.
   * @param {!RouteMessage} message
   * @return {!mediaRouterMojom.RouteMessage} A Mojo RouteMessage object.
   */
  function messageToMojo_(message) {
    if ("string" == typeof message.message) {
      return new mediaRouterMojom.RouteMessage({
        'type': mediaRouterMojom.RouteMessage.Type.TEXT,
        'message': message.message,
      });
    } else {
      return new mediaRouterMojom.RouteMessage({
        'type': mediaRouterMojom.RouteMessage.Type.BINARY,
        'data': message.message,
      });
    }
  }

  /**
   * Converts presentation connection state to Mojo enum value.
   * @param {!string} state
   * @return {!mediaRouterMojom.MediaRouter.PresentationConnectionState}
   */
  function presentationConnectionStateToMojo_(state) {
    var PresentationConnectionState =
        mediaRouterMojom.MediaRouter.PresentationConnectionState;
    switch (state) {
      case 'connecting':
        return PresentationConnectionState.CONNECTING;
      case 'connected':
        return PresentationConnectionState.CONNECTED;
      case 'closed':
        return PresentationConnectionState.CLOSED;
      case 'terminated':
        return PresentationConnectionState.TERMINATED;
      default:
        console.error('Unknown presentation connection state: ' + state);
        return PresentationConnectionState.TERMINATED;
    }
  }

  /**
   * Converts presentation connection close reason to Mojo enum value.
   * @param {!string} reason
   * @return {!mediaRouterMojom.MediaRouter.PresentationConnectionCloseReason}
   */
  function presentationConnectionCloseReasonToMojo_(reason) {
    var PresentationConnectionCloseReason =
        mediaRouterMojom.MediaRouter.PresentationConnectionCloseReason;
    switch (reason) {
      case 'error':
        return PresentationConnectionCloseReason.CONNECTION_ERROR;
      case 'closed':
        return PresentationConnectionCloseReason.CLOSED;
      case 'went_away':
        return PresentationConnectionCloseReason.WENT_AWAY;
      default:
        console.error('Unknown presentation connection close reason : ' +
            reason);
        return PresentationConnectionCloseReason.CONNECTION_ERROR;
    }
  }

  // TODO(crbug.com/688177): remove this conversion when M60 is in stable.
  /**
   * Converts string to Mojo origin.
   * @param {string|!originMojom.Origin} origin
   * @return {!originMojom.Origin}
   */
  function stringToMojoOrigin_(origin) {
    if (origin instanceof originMojom.Origin) {
      return origin;
    }
    const url = new URL(origin);
    const mojoOrigin = {};
    mojoOrigin.scheme = url.protocol.replace(':', '');
    mojoOrigin.host = url.hostname;
    var port = url.port ? Number.parseInt(url.port) : 0;
    switch (mojoOrigin.scheme) {
      case 'http':
        mojoOrigin.port = port || 80;
        break;
      case 'https':
        mojoOrigin.port = port || 443;
        break;
      default:
        throw new Error('Scheme must be http or https');
    }
    mojoOrigin.suborigin = '';
    return new originMojom.Origin(mojoOrigin);
  }

  /**
   * Parses the given route request Error object and converts it to the
   * corresponding result code.
   * @param {!Error} error
   * @return {!mediaRouterMojom.RouteRequestResultCode}
   */
  function getRouteRequestResultCode_(error) {
    return error.errorCode ? error.errorCode :
      mediaRouterMojom.RouteRequestResultCode.UNKNOWN_ERROR;
  }

  /**
   * Creates and returns a successful route response from given route.
   * @param {!MediaRoute} route
   * @return {!Object}
   */
  function toSuccessRouteResponse_(route) {
    return {
        route: routeToMojo_(route),
        result_code: mediaRouterMojom.RouteRequestResultCode.OK
    };
  }

  /**
   * Creates and returns a error route response from given Error object.
   * @param {!Error} error
   * @return {!Object}
   */
  function toErrorRouteResponse_(error) {
    return {
        error_text: error.message,
        result_code: getRouteRequestResultCode_(error)
    };
  }

  /**
   * Creates a new MediaRouter.
   * Converts a route struct to its Mojo form.
   * @param {!mediaRouterMojom.MediaRouterPtr} service
   * @constructor
   */
  function MediaRouter(service) {
    /**
     * The Mojo service proxy. Allows extension code to call methods that reside
     * in the browser.
     * @type {!mediaRouterMojom.MediaRouterPtr}
     */
    this.service_ = service;

    /**
     * The provider manager service delegate. Its methods are called by the
     * browser-resident Mojo service.
     * @type {!MediaRouter}
     */
    this.mrpm_ = new MediaRouteProvider(this);

    /**
     * Handle to a KeepAlive service object, which prevents the extension from
     * being suspended as long as it remains in scope.
     * @type {boolean}
     */
    this.keepAlive_ = null;

    /**
     * The bindings to bind the service delegate to the Mojo interface.
     * Object must remain in scope for the lifetime of the connection to
     * prevent the connection from closing automatically.
     * @type {!bindings.Binding}
     */
    this.mediaRouteProviderBinding_ = new bindings.Binding(
        mediaRouterMojom.MediaRouteProvider, this.mrpm_);
  }

  /**
   * Returns definitions of Mojo core and generated Mojom classes that can be
   * used directly by the component.
   * @return {!Object}
   * TODO(imcheng): We should export these along with MediaRouter. This requires
   * us to modify the component to handle multiple exports. When that logic is
   * baked in for a couple of milestones, we should be able to remove this
   * method.
   */
  MediaRouter.prototype.getMojoExports = function() {
    return {
      Binding: bindings.Binding,
      DialMediaSink: mediaRouterMojom.DialMediaSink,
      CastMediaSink: mediaRouterMojom.CastMediaSink,
      IPAddress: ipAddressMojom.IPAddress,
      InterfacePtrController: bindings.InterfacePtrController,
      InterfaceRequest: bindings.InterfaceRequest,
      MediaController: mediaControllerMojom.MediaController,
      MediaStatus: mediaStatusMojom.MediaStatus,
      MediaStatusObserverPtr: mediaStatusMojom.MediaStatusObserverPtr,
      MirrorServiceRemoter: remotingMojom.MirrorServiceRemoter,
      MirrorServiceRemoterPtr: remotingMojom.MirrorServiceRemoterPtr,
      MirrorServiceRemotingSourcePtr:
          remotingMojom.MirrorServiceRemotingSourcePtr,
      RemotingStopReason: remotingCommonMojom.RemotingStopReason,
      RemotingStartFailReason: remotingCommonMojom.RemotingStartFailReason,
      RemotingSinkFeatures: remotingCommonMojom.RemotingSinkFeatures,
      RemotingSinkAudioCapabilities:
          remotingCommonMojom.RemotingSinkAudioCapabilities,
      RemotingSinkVideoCapabilities:
          remotingCommonMojom.RemotingSinkVideoCapabilities,
      SinkCapabilities: remotingCommonMojom.SinkCapabilities,
      Origin: originMojom.Origin,
      Sink: mediaRouterMojom.MediaSink,
      SinkExtraData: mediaRouterMojom.MediaSinkExtraData,
      TimeDelta: timeMojom.TimeDelta,
      Url: urlMojom.Url,
      makeRequest: bindings.makeRequest,
    };
  };

  /**
   * Registers the Media Router Provider Manager with the Media Router.
   * @return {!Promise<Object>} Instance ID and config for the Media Router.
   */
  MediaRouter.prototype.start = function() {
    return this.service_.registerMediaRouteProvider(
        this.mediaRouteProviderBinding_.createInterfacePtrAndBind());
  }

  /**
   * Sets the service delegate methods.
   * @param {Object} handlers
   */
  MediaRouter.prototype.setHandlers = function(handlers) {
    this.mrpm_.setHandlers(handlers);
  }

  /**
   * The keep alive status.
   * @return {boolean}
   */
  MediaRouter.prototype.getKeepAlive = function() {
    return this.keepAlive_ != null;
  };

  /**
   * Called by the provider manager when a sink list for a given source is
   * updated.
   * @param {!string} sourceUrn
   * @param {!Array<!MediaSink>} sinks
   * @param {!Array<string>} origins
   */
  MediaRouter.prototype.onSinksReceived = function(sourceUrn, sinks,
      origins) {
    this.service_.onSinksReceived(sourceUrn, sinks.map(sinkToMojo_),
        origins.map(stringToMojoOrigin_));
  };

  /**
   * Called by the provider manager when a sink is found to notify the MR of the
   * sink's ID. The actual sink will be returned through the normal sink list
   * update process, so this helps the MR identify the search result in the
   * list.
   * @param {string} pseudoSinkId  ID of the pseudo sink that started the
   *     search.
   * @param {string} sinkId ID of the newly-found sink.
   */
  MediaRouter.prototype.onSearchSinkIdReceived = function(
      pseudoSinkId, sinkId) {
    this.service_.onSearchSinkIdReceived(pseudoSinkId, sinkId);
  };

  /**
   * Called by the provider manager to keep the extension from suspending
   * if it enters a state where suspension is undesirable (e.g. there is an
   * active MediaRoute.)
   * If keepAlive is true, the extension is kept alive.
   * If keepAlive is false, the extension is allowed to suspend.
   * @param {boolean} keepAlive
   */
  MediaRouter.prototype.setKeepAlive = function(keepAlive) {
    if (keepAlive === false && this.keepAlive_) {
      this.keepAlive_.ptr.reset();
      this.keepAlive_ = null;
    } else if (keepAlive === true && !this.keepAlive_) {
      this.keepAlive_ = new keepAliveMojom.KeepAlivePtr(
          frameInterfaces.getInterface(keepAliveMojom.KeepAlive.name));
    }
  };

  /**
   * Called by the provider manager to send an issue from a media route
   * provider to the Media Router, to show the user.
   * @param {!Object} issue The issue object.
   */
  MediaRouter.prototype.onIssue = function(issue) {
    function issueSeverityToMojo_(severity) {
      switch (severity) {
        case 'fatal':
          return mediaRouterMojom.Issue.Severity.FATAL;
        case 'warning':
          return mediaRouterMojom.Issue.Severity.WARNING;
        case 'notification':
          return mediaRouterMojom.Issue.Severity.NOTIFICATION;
        default:
          console.error('Unknown issue severity: ' + severity);
          return mediaRouterMojom.Issue.Severity.NOTIFICATION;
      }
    }

    function issueActionToMojo_(action) {
      switch (action) {
        case 'dismiss':
          return mediaRouterMojom.Issue.ActionType.DISMISS;
        case 'learn_more':
          return mediaRouterMojom.Issue.ActionType.LEARN_MORE;
        default:
          console.error('Unknown issue action type : ' + action);
          return mediaRouterMojom.Issue.ActionType.OK;
      }
    }

    var secondaryActions = (issue.secondaryActions || []).map(function(e) {
      return issueActionToMojo_(e);
    });
    this.service_.onIssue(new mediaRouterMojom.Issue({
      'route_id': issue.routeId,
      'severity': issueSeverityToMojo_(issue.severity),
      'title': issue.title,
      'message': issue.message,
      'default_action': issueActionToMojo_(issue.defaultAction),
      'secondary_actions': secondaryActions,
      'help_page_id': issue.helpPageId,
      'is_blocking': issue.isBlocking
    }));
  };

  /**
   * Called by the provider manager when the set of active routes
   * has been updated.
   * @param {!Array<MediaRoute>} routes The active set of media routes.
   * @param {string=} sourceUrn The sourceUrn associated with this route
   *     query.
   * @param {Array<string>=} joinableRouteIds The active set of joinable
   *     media routes.
   */
  MediaRouter.prototype.onRoutesUpdated =
      function(routes, sourceUrn = '', joinableRouteIds = []) {
    this.service_.onRoutesUpdated(
        routes.map(routeToMojo_),
        sourceUrn,
        joinableRouteIds);
  };

  /**
   * Called by the provider manager when sink availability has been updated.
   * @param {!mediaRouterMojom.MediaRouter.SinkAvailability} availability
   *     The new sink availability.
   */
  MediaRouter.prototype.onSinkAvailabilityUpdated = function(availability) {
    this.service_.onSinkAvailabilityUpdated(availability);
  };

  /**
   * Called by the provider manager when the state of a presentation connected
   * to a route has changed.
   * @param {string} routeId
   * @param {string} state
   */
  MediaRouter.prototype.onPresentationConnectionStateChanged =
      function(routeId, state) {
    this.service_.onPresentationConnectionStateChanged(
        routeId, presentationConnectionStateToMojo_(state));
  };

  /**
   * Called by the provider manager when the state of a presentation connected
   * to a route has closed.
   * @param {string} routeId
   * @param {string} reason
   * @param {string} message
   */
  MediaRouter.prototype.onPresentationConnectionClosed =
      function(routeId, reason, message) {
    this.service_.onPresentationConnectionClosed(
        routeId, presentationConnectionCloseReasonToMojo_(reason), message);
  };

  /**
   * @param {string} routeId
   * @param {!Array<!RouteMessage>} mesages
   */
  MediaRouter.prototype.onRouteMessagesReceived = function(routeId, messages) {
    this.service_.onRouteMessagesReceived(
        routeId, messages.map(messageToMojo_));
  };

  /**
   * @param {number} tabId
   * @param {!remotingMojom.MirrorServiceRemoterPtr} remoter
   * @param {!remotingMojom.MirrorServiceRemotingSourcePtr} remotingSource
   */
  MediaRouter.prototype.onMediaRemoterCreated = function(tabId, remoter,
      remotingSource) {
    this.service_.onMediaRemoterCreated(tabId, remoter, remotingSource);
  }

  /**
   * Object containing callbacks set by the provider manager.
   *
   * @constructor
   * @struct
   */
  function MediaRouterHandlers() {
    /**
     * @type {function(!string, !string, !string, !string, !number)}
     */
    this.createRoute = null;

    /**
     * @type {function(!string, !string, !string, !number)}
     */
    this.joinRoute = null;

    /**
     * @type {function(string): Promise}
     */
    this.terminateRoute = null;

    /**
     * @type {function(string)}
     */
    this.startObservingMediaSinks = null;

    /**
     * @type {function(string)}
     */
    this.stopObservingMediaSinks = null;

    /**
     * @type {function(string, string): Promise}
     */
    this.sendRouteMessage = null;

    /**
     * @type {function(string, Uint8Array): Promise}
     */
    this.sendRouteBinaryMessage = null;

    /**
     * @type {function(string)}
     */
    this.startListeningForRouteMessages = null;

    /**
     * @type {function(string)}
     */
    this.stopListeningForRouteMessages = null;

    /**
     * @type {function(string)}
     */
    this.detachRoute = null;

    /**
     * @type {function()}
     */
    this.startObservingMediaRoutes = null;

    /**
     * @type {function()}
     */
    this.stopObservingMediaRoutes = null;

    /**
     * @type {function()}
     */
    this.connectRouteByRouteId = null;

    /**
     * @type {function()}
     */
    this.enableMdnsDiscovery = null;

    /**
     * @type {function()}
     */
    this.updateMediaSinks = null;

    /**
     * @type {function(string, string, !SinkSearchCriteria): string}
     */
    this.searchSinks = null;

    /**
     * @type {function()}
     */
    this.provideSinks = null;

    /**
     * @type {function(string, !bindings.InterfaceRequest,
     *            !mediaStatusMojom.MediaStatusObserverPtr): !Promise<void>}
     */
    this.createMediaRouteController = null;
  };

  /**
   * Routes calls from Media Router to the provider manager extension.
   * Registered with the MediaRouter stub.
   * @param {!MediaRouter} MediaRouter proxy to call into the
   * Media Router mojo interface.
   * @constructor
   */
  function MediaRouteProvider(mediaRouter) {
    /**
     * Object containing JS callbacks into Provider Manager code.
     * @type {!MediaRouterHandlers}
     */
    this.handlers_ = new MediaRouterHandlers();

    /**
     * Proxy class to the browser's Media Router Mojo service.
     * @type {!MediaRouter}
     */
    this.mediaRouter_ = mediaRouter;
  }

  /*
   * Sets the callback handler used to invoke methods in the provider manager.
   *
   * @param {!MediaRouterHandlers} handlers
   */
  MediaRouteProvider.prototype.setHandlers = function(handlers) {
    this.handlers_ = handlers;
    var requiredHandlers = [
      'stopObservingMediaRoutes',
      'startObservingMediaRoutes',
      'sendRouteMessage',
      'sendRouteBinaryMessage',
      'startListeningForRouteMessages',
      'stopListeningForRouteMessages',
      'detachRoute',
      'terminateRoute',
      'joinRoute',
      'createRoute',
      'stopObservingMediaSinks',
      'startObservingMediaRoutes',
      'connectRouteByRouteId',
      'enableMdnsDiscovery',
      'updateMediaSinks',
      'searchSinks',
      'provideSinks',
      'createMediaRouteController',
      'onBeforeInvokeHandler'
    ];
    requiredHandlers.forEach(function(nextHandler) {
      if (handlers[nextHandler] === undefined) {
        console.error(nextHandler + ' handler not registered.');
      }
    });
  }

  /**
   * Starts querying for sinks capable of displaying the media source
   * designated by |sourceUrn|.  Results are returned by calling
   * OnSinksReceived.
   * @param {!string} sourceUrn
   */
  MediaRouteProvider.prototype.startObservingMediaSinks =
      function(sourceUrn) {
    this.handlers_.onBeforeInvokeHandler();
    this.handlers_.startObservingMediaSinks(sourceUrn);
  };

  /**
   * Stops querying for sinks capable of displaying |sourceUrn|.
   * @param {!string} sourceUrn
   */
  MediaRouteProvider.prototype.stopObservingMediaSinks =
      function(sourceUrn) {
    this.handlers_.onBeforeInvokeHandler();
    this.handlers_.stopObservingMediaSinks(sourceUrn);
  };

  /**
   * Requests that |sinkId| render the media referenced by |sourceUrn|. If the
   * request is from the Presentation API, then origin and tabId will
   * be populated.
   * @param {!string} sourceUrn Media source to render.
   * @param {!string} sinkId Media sink ID.
   * @param {!string} presentationId Presentation ID from the site
   *     requesting presentation. TODO(mfoltz): Remove.
   * @param {!originMojom.Origin} origin Origin of site requesting presentation.
   * @param {!number} tabId ID of tab requesting presentation.
   * @param {!timeMojom.TimeDelta} timeout If positive, the timeout duration for
   *     the request. Otherwise, the default duration will be used.
   * @param {!boolean} incognito If true, the route is being requested by
   *     an incognito profile.
   * @return {!Promise.<!Object>} A Promise resolving to an object describing
   *     the newly created media route, or rejecting with an error message on
   *     failure.
   */
  MediaRouteProvider.prototype.createRoute =
      function(sourceUrn, sinkId, presentationId, origin, tabId,
               timeout, incognito) {
    this.handlers_.onBeforeInvokeHandler();
    return this.handlers_.createRoute(
        sourceUrn, sinkId, presentationId, origin, tabId,
        Math.floor(timeout.microseconds / 1000), incognito)
        .then(function(route) {
          return toSuccessRouteResponse_(route);
        },
        function(err) {
          return toErrorRouteResponse_(err);
        });
  };

  /**
   * Handles a request via the Presentation API to join an existing route given
   * by |sourceUrn| and |presentationId|. |origin| and |tabId| are used for
   * validating same-origin/tab scope.
   * @param {!string} sourceUrn Media source to render.
   * @param {!string} presentationId Presentation ID to join.
   * @param {!originMojom.Origin} origin Origin of site requesting join.
   * @param {!number} tabId ID of tab requesting join.
   * @param {!timeMojom.TimeDelta} timeout If positive, the timeout duration for
   *     the request. Otherwise, the default duration will be used.
   * @param {!boolean} incognito If true, the route is being requested by
   *     an incognito profile.
   * @return {!Promise.<!Object>} A Promise resolving to an object describing
   *     the newly created media route, or rejecting with an error message on
   *     failure.
   */
  MediaRouteProvider.prototype.joinRoute =
      function(sourceUrn, presentationId, origin, tabId, timeout,
               incognito) {
    this.handlers_.onBeforeInvokeHandler();
    return this.handlers_.joinRoute(
        sourceUrn, presentationId, origin, tabId,
        Math.floor(timeout.microseconds / 1000), incognito)
        .then(function(route) {
          return toSuccessRouteResponse_(route);
        },
        function(err) {
          return toErrorRouteResponse_(err);
        });
  };

  /**
   * Handles a request via the Presentation API to join an existing route given
   * by |sourceUrn| and |routeId|. |origin| and |tabId| are used for
   * validating same-origin/tab scope.
   * @param {!string} sourceUrn Media source to render.
   * @param {!string} routeId Route ID to join.
   * @param {!string} presentationId Presentation ID to join.
   * @param {!originMojom.Origin} origin Origin of site requesting join.
   * @param {!number} tabId ID of tab requesting join.
   * @param {!timeMojom.TimeDelta} timeout If positive, the timeout duration for
   *     the request. Otherwise, the default duration will be used.
   * @param {!boolean} incognito If true, the route is being requested by
   *     an incognito profile.
   * @return {!Promise.<!Object>} A Promise resolving to an object describing
   *     the newly created media route, or rejecting with an error message on
   *     failure.
   */
  MediaRouteProvider.prototype.connectRouteByRouteId =
      function(sourceUrn, routeId, presentationId, origin, tabId,
               timeout, incognito) {
    this.handlers_.onBeforeInvokeHandler();
    return this.handlers_.connectRouteByRouteId(
        sourceUrn, routeId, presentationId, origin, tabId,
        Math.floor(timeout.microseconds / 1000), incognito)
        .then(function(route) {
          return toSuccessRouteResponse_(route);
        },
        function(err) {
          return toErrorRouteResponse_(err);
        });
  };

  /**
   * Terminates the route specified by |routeId|.
   * @param {!string} routeId
   * @return {!Promise<!Object>} A Promise resolving to an object describing
   *    the result of the terminate operation, or rejecting with an error
   *    message and code if the operation failed.
   */
  MediaRouteProvider.prototype.terminateRoute = function(routeId) {
    this.handlers_.onBeforeInvokeHandler();
    return this.handlers_.terminateRoute(routeId).then(
        () => ({result_code: mediaRouterMojom.RouteRequestResultCode.OK}),
        (err) => toErrorRouteResponse_(err));
  };

  /**
   * Posts a message to the route designated by |routeId|.
   * @param {!string} routeId
   * @param {!string} message
   * @return {!Promise.<boolean>} Resolved with true if the message was sent,
   *    or false on failure.
   */
  MediaRouteProvider.prototype.sendRouteMessage = function(
    routeId, message) {
    this.handlers_.onBeforeInvokeHandler();
    return this.handlers_.sendRouteMessage(routeId, message)
        .then(function() {
          return {'sent': true};
        }, function() {
          return {'sent': false};
        });
  };

  /**
   * Sends a binary message to the route designated by |routeId|.
   * @param {!string} routeId
   * @param {!Array<number>} data
   * @return {!Promise.<boolean>} Resolved with true if the data was sent,
   *    or false on failure.
   */
  MediaRouteProvider.prototype.sendRouteBinaryMessage = function(
    routeId, data) {
    this.handlers_.onBeforeInvokeHandler();
    return this.handlers_.sendRouteBinaryMessage(routeId, new Uint8Array(data))
        .then(function() {
          return {'sent': true};
        }, function() {
          return {'sent': false};
        });
  };

  /**
   * Listen for messages from a route.
   * @param {!string} routeId
   */
  MediaRouteProvider.prototype.startListeningForRouteMessages = function(
      routeId) {
    this.handlers_.onBeforeInvokeHandler();
    this.handlers_.startListeningForRouteMessages(routeId);
  };

  /**
   * @param {!string} routeId
   */
  MediaRouteProvider.prototype.stopListeningForRouteMessages = function(
      routeId) {
    this.handlers_.onBeforeInvokeHandler();
    this.handlers_.stopListeningForRouteMessages(routeId);
  };

  /**
   * Indicates that the presentation connection that was connected to |routeId|
   * is no longer connected to it.
   * @param {!string} routeId
   */
  MediaRouteProvider.prototype.detachRoute = function(
      routeId) {
    this.handlers_.detachRoute(routeId);
  };

  /**
   * Requests that the provider manager start sending information about active
   * media routes to the Media Router.
   * @param {!string} sourceUrn
   */
  MediaRouteProvider.prototype.startObservingMediaRoutes = function(sourceUrn) {
    this.handlers_.onBeforeInvokeHandler();
    this.handlers_.startObservingMediaRoutes(sourceUrn);
  };

  /**
   * Requests that the provider manager stop sending information about active
   * media routes to the Media Router.
   * @param {!string} sourceUrn
   */
  MediaRouteProvider.prototype.stopObservingMediaRoutes = function(sourceUrn) {
    this.handlers_.onBeforeInvokeHandler();
    this.handlers_.stopObservingMediaRoutes(sourceUrn);
  };

  /**
   * Enables mDNS device discovery.
   */
  MediaRouteProvider.prototype.enableMdnsDiscovery = function() {
    this.handlers_.onBeforeInvokeHandler();
    this.handlers_.enableMdnsDiscovery();
  };

  /**
   * Requests that the provider manager update media sinks.
   * @param {!string} sourceUrn
   */
  MediaRouteProvider.prototype.updateMediaSinks = function(sourceUrn) {
    this.handlers_.onBeforeInvokeHandler();
    this.handlers_.updateMediaSinks(sourceUrn);
  };

  /**
   * Requests that the provider manager search its providers for a sink matching
   * |searchCriteria| that is compatible with |sourceUrn|. If a sink is found
   * that can be used immediately for route creation, its ID is returned.
   * Otherwise the empty string is returned.
   *
   * @param {string} sinkId Sink ID of the pseudo sink generating the request.
   * @param {string} sourceUrn Media source to be used with the sink.
   * @param {!SinkSearchCriteria} searchCriteria Search criteria for the route
   *     providers.
   * @return {!Promise.<!{sink_id: !string}>} A Promise resolving to either the
   *     sink ID of the sink found by the search that can be used for route
   *     creation, or the empty string if no route can be immediately created.
   */
  MediaRouteProvider.prototype.searchSinks = function(
      sinkId, sourceUrn, searchCriteria) {
    this.handlers_.onBeforeInvokeHandler();
    const searchSinksResponse =
        this.handlers_.searchSinks(sinkId, sourceUrn, searchCriteria);

    if ('string' == typeof searchSinksResponse) {
        // TODO (zijiang): Remove this check when M59 is stable and the
        // extension is always returning a promise.
        return Promise.resolve({
          'sink_id': sink_id
        });
    }
    return searchSinksResponse.then(
        sink_id => {
          return { 'sink_id': sink_id };
        },
        () => {
          return { 'sink_id': '' };
        });
  };

  /**
   * Notifies the provider manager that MediaRouter has discovered a list of
   * sinks.
   * @param {string} providerName
   * @param {!Array<!mediaRouterMojom.MediaSink>} sinks
   */
  MediaRouteProvider.prototype.provideSinks = function(providerName, sinks) {
    this.handlers_.onBeforeInvokeHandler();
    this.handlers_.provideSinks(providerName, sinks);
  };

  /**
   * Creates a controller for the given route and binds the given
   * InterfaceRequest to it, and registers an observer for media status updates
   * for the route.
   * @param {string} routeId
   * @param {!bindings.InterfaceRequest} controllerRequest
   * @param {!mediaStatusMojom.MediaStatusObserverPtr} observer
   * @return {!Promise<!{success: boolean}>} Resolves to true if a controller
   *     is created. Resolves to false if a controller cannot be created, or if
   *     the controller is already bound.
   */
  MediaRouteProvider.prototype.createMediaRouteController = function(
      routeId, controllerRequest, observer) {
    // TODO(imcheng): Remove this check when M60 is in stable.
    if (!this.handlers_.createMediaRouteController) {
      return Promise.resolve({success: false});
    }

    this.handlers_.onBeforeInvokeHandler();
    return this.handlers_
        .createMediaRouteController(routeId, controllerRequest, observer)
        .then(() => ({success: true}), e => ({success: false}));
  };

  mediaRouter = new MediaRouter(new mediaRouterMojom.MediaRouterPtr(
      frameInterfaces.getInterface(mediaRouterMojom.MediaRouter.name)));

  return mediaRouter;
});
