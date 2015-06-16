// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var mediaRouterObserver;

define('media_router_bindings', [
    'mojo/public/js/bindings',
    'mojo/public/js/core',
    'content/public/renderer/service_provider',
    'chrome/browser/media/router/media_router.mojom',
    'extensions/common/mojo/keep_alive.mojom',
    'mojo/public/js/connection',
    'mojo/public/js/router',
], function(bindings,
            core,
            serviceProvider,
            mediaRouterMojom,
            keepAliveMojom,
            connector,
            routerModule) {
  'use strict';

  /**
   * Converts a media sink to a MediaSink Mojo object.
   * @param {!MediaSink} sink A media sink.
   * @return {!mediaRouterMojom.MediaSink} A Mojo MediaSink object.
   */
  function sinkToMojo_(sink) {
    return new mediaRouterMojom.MediaSink({
      'name': sink.friendlyName,
      'sink_id': sink.id,
    });
  }

  /**
   * Returns a Mojo MediaRoute object given a MediaRoute and a
   * media sink name.
   * @param {!MediaRoute} route
   * @param {!string} sinkName
   * @return {!mojo.MediaRoute}
   */
  function routeToMojo_(route, sinkName) {
    return new mediaRouterMojom.MediaRoute({
      'media_route_id': route.id,
      'media_source': route.mediaSource,
      'media_sink': new mediaRouterMojom.MediaSink({
        'sink_id': route.sinkId,
        'name': sinkName,
      }),
      'description': route.description,
      'icon_url': route.iconUrl,
      'is_local': route.isLocal
    });
  }

  /**
   * Creates a new MediaRouterObserver.
   * Converts a route struct to its Mojo form.
   * @param {!MediaRouterService} service
   * @constructor
   */
  function MediaRouterObserver(service) {
    /**
     * The Mojo service proxy. Allows extension code to call methods that reside
     * in the browser.
     * @type {!MediaRouterService}
     */
    this.service_ = service;

    /**
     * The provider manager service delegate. Its methods are called by the
     * browser-resident Mojo service.
     * @type {!MediaRouter}
     */
    this.mrpm_ = new MediaRouter(this);

    /**
     * The message pipe that connects the Media Router to mrpm_ across
     * browser/renderer IPC boundaries. Object must remain in scope for the
     * lifetime of the connection to prevent the connection from closing
     * automatically.
     * @type {!mojo.MessagePipe}
     */
    this.pipe_ = core.createMessagePipe();

    /**
     * Handle to a KeepAlive service object, which prevents the extension from
     * being suspended as long as it remains in scope.
     * @type {boolean}
     */
    this.keepAlive_ = null;

    /**
     * The stub used to bind the service delegate to the Mojo interface.
     * Object must remain in scope for the lifetime of the connection to
     * prevent the connection from closing automatically.
     * @type {!mojom.MediaRouter}
     */
    this.mediaRouterStub_ = connector.bindHandleToStub(
        this.pipe_.handle0, mediaRouterMojom.MediaRouter);

    // Link mediaRouterStub_ to the provider manager delegate.
    bindings.StubBindings(this.mediaRouterStub_).delegate = this.mrpm_;
  }

  /**
   * Registers the Media Router Provider Manager with the Media Router.
   * @return {!Promise<string>} Instance ID for the Media Router.
   */
  MediaRouterObserver.prototype.start = function() {
    return this.service_.provideMediaRouter(this.pipe_.handle1).then(
        function(result) {
          return result.instance_id;
        }.bind(this));
  }

  /**
   * Sets the service delegate methods.
   * @param {Object} handlers
   */
  MediaRouterObserver.prototype.setHandlers = function(handlers) {
    this.mrpm_.setHandlers(handlers);
  }

  /**
   * The keep alive status.
   * @return {boolean}
   */
  MediaRouterObserver.prototype.getKeepAlive = function() {
    return this.keepAlive_ != null;
  };

  /**
   * Called by the provider manager when a sink list for a given source is
   * updated.
   * @param {!string} sourceUrn
   * @param {!Array<!MediaSink>} sinks
   */
  MediaRouterObserver.prototype.onSinksReceived = function(sourceUrn, sinks) {
    this.service_.onSinksReceived(sourceUrn, sinks.map(sinkToMojo_));
  };

  /**
   * Called by the provider manager to keep the extension from suspending
   * if it enters a state where suspension is undesirable (e.g. there is an
   * active MediaRoute.)
   * If keepAlive is true, the extension is kept alive.
   * If keepAlive is false, the extension is allowed to suspend.
   * @param {boolean} keepAlive
   */
  MediaRouterObserver.prototype.setKeepAlive = function(keepAlive) {
    if (keepAlive === false && this.keepAlive_) {
      this.keepAlive_.close();
      this.keepAlive_ = null;
    } else if (keepAlive === true && !this.keepAlive_) {
      this.keepAlive_ = new routerModule.Router(
          serviceProvider.connectToService(
              keepAliveMojom.KeepAlive.name));
    }
  };

  /**
   * Sends a message to an active media route.
   * @param {!string} routeId
   * @param {!Object|string} message A message that can be converted to a JSON
   * string.
   */
  MediaRouterObserver.prototype.onMessage = function(routeId, message) {
    // TODO(mfoltz): Handle binary messages (ArrayBuffer, Blob).
    this.service_.onMessage(routeId, JSON.stringify(message));
  };

  /**
   * Called by the provider manager to send an issue from a media route
   * provider to the Media Router, to show the user.
   * @param {!Object} issue The issue object.
   */
  MediaRouterObserver.prototype.onIssue = function(issue) {
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
        case 'ok':
          return mediaRouterMojom.Issue.ActionType.OK;
        case 'cancel':
          return mediaRouterMojom.Issue.ActionType.CANCEL;
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
      'help_url': issue.helpUrl,
      'is_blocking': issue.isBlocking
    }));
  };

  /**
   * Called by the provider manager when the set of active routes
   * has been updated.
   * @param {!Array<MediaRoute>} routes The active set of media routes.
   * @param {!Array<MediaSink>} sinks The active set of media sinks.
   */
  MediaRouterObserver.prototype.onRoutesUpdated = function(routes, sinks) {
    // Create an inverted index relating sink IDs to their names.
    var sinkNameMap = {};
    for (var i = 0; i < sinks.length; i++) {
      sinkNameMap[sinks[i].id] = sinks[i].friendlyName;
    }

    // Convert MediaRoutes to Mojo objects and add their sink names
    // via sinkNameMap.
    var mojoRoutes = routes.map(function(nextRoute) {
      return routeToMojo_(nextRoute, sinkNameMap[nextRoute.sinkId]);
    });

    this.service_.onRoutesUpdated(mojoRoutes, sinks.map(sinkToMojo_));
  };

  /**
   * Called by the Provider Manager when an error was encountered in response
   * to a media route creation request.
   * @param {!string} requestId The request id.
   * @param {!string} error The error.
   */
  MediaRouterObserver.prototype.onRouteResponseError =
      function(requestId, error) {
    this.service_.onRouteResponseError(requestId, error);
  };

  /**
   * Called by the provider manager when a route was able to be created by a
   * media route provider.
   *
   * @param {string} requestId The media route request id.
   * @param {string} routeId The id of the media route that was created.
   */
  MediaRouterObserver.prototype.onRouteResponseReceived =
      function(requestId, routeId) {
    this.service_.onRouteResponseReceived(requestId, routeId);
  };

  /**
   * Object containing callbacks set by the provider manager.
   * TODO(mfoltz): Better named ProviderManagerDelegate?
   *
   * @constructor
   * @struct
   */
  function MediaRouterHandlers() {
    /**
     * @type {function(!string, !string, !string, !string, !number}
     */
    this.createRoute = null;

    /**
     * @type {function(!string, !string, !string, !number)}
     */
    this.joinRoute = null;

    /**
     * @type {function(string)}
     */
    this.closeRoute = null;

    /**
     * @type {function(string)}
     */
    this.startObservingMediaSinks = null;

    /**
     * @type {function(string)}
     */
    this.stopObservingMediaSinks = null;

    /**
     * @type {function(string, string)}
     */
    this.sendRouteMessage = null;

    /**
     * @type {function()}
     */
    this.startObservingMediaRoutes = null;

    /**
     * @type {function()}
     */
    this.stopObservingMediaRoutes = null;
  };

  /**
   * Routes calls from Media Router to the provider manager extension.
   * Registered with the MediaRouter stub.
   * @param {!MediaRouterObserver} mediaRouterObserver API to call into the
   * Media Router mojo interface.
   * @constructor
   */
  function MediaRouter(mediaRouterObserver) {
    mediaRouterMojom.MediaRouter.stubClass.call(this);

    /**
     * Object containing JS callbacks into Provider Manager code.
     * @type {!MediaRouterHandlers}
     */
    this.handlers_ = new MediaRouterHandlers();

    /**
     * Proxy class to the browser's Media Router Mojo service.
     * @type {!MediaRouterObserver}
     */
    this.mediaRouter_ = mediaRouterObserver;
  }
  MediaRouter.prototype = Object.create(
      mediaRouterMojom.MediaRouter.stubClass.prototype);

  /*
   * Sets the callback handler used to invoke methods in the provider manager.
   *
   * TODO(mfoltz): Rename to something more explicit?
   * @param {!MediaRouterHandlers} handlers
   */
  MediaRouter.prototype.setHandlers = function(handlers) {
    this.handlers_ = handlers;
    var requiredHandlers = [
      'stopObservingMediaRoutes',
      'startObservingMediaRoutes',
      'sendRouteMessage',
      'closeRoute',
      'joinRoute',
      'createRoute',
      'stopObservingMediaSinks',
      'startObservingMediaRoutes'
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
  MediaRouter.prototype.startObservingMediaSinks =
      function(sourceUrn) {
    this.handlers_.startObservingMediaSinks(sourceUrn);
  };

  /**
   * Stops querying for sinks capable of displaying |sourceUrn|.
   * @param {!string} sourceUrn
   */
  MediaRouter.prototype.stopObservingMediaSinks =
      function(sourceUrn) {
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
   * @param {!string} origin Origin of site requesting presentation.
   * @param {!number} tabId ID of tab requesting presentation.
   * @return {!Promise.<!Object>} A Promise resolving to an object describing
   *     the newly created media route, or rejecting with an error message on
   *     failure.
   */
  MediaRouter.prototype.createRoute =
      function(sourceUrn, sinkId, presentationId, origin, tabId) {
    return this.handlers_.createRoute(
        sourceUrn, sinkId, presentationId, origin, tabId)
        .then(function(route) {
          // Sink name is not used, so it is omitted here.
          return {route: routeToMojo_(route, "")};
        }.bind(this))
        .catch(function(err) {
          return {error_text: 'Error creating route: ' + err.message};
        });
  };

  /**
   * Handles a request via the Presentation API to join an existing route given
   * by |sourceUrn| and |presentationId|. |origin| and |tabId| are used for
   * validating same-origin/tab scope.
   * @param {!string} sourceUrn Media source to render.
   * @param {!string} presentationId Presentation ID to join.
   * @param {!string} origin Origin of site requesting join.
   * @param {!number} tabId ID of tab requesting join.
   * @return {!Promise.<!Object>} A Promise resolving to an object describing
   *     the newly created media route, or rejecting with an error message on
   *     failure.
   */
  MediaRouter.prototype.joinRoute =
      function(sourceUrn, presentationId, origin, tabId) {
    return this.handlers_.joinRoute(sourceUrn, presentationId, origin, tabId)
        .then(function(newRoute) {
          // Sink name is not used, so it is omitted here.
          return {route: routeToMojo_(newRoute, "")};
        },
        function(err) {
          return {error_text: 'Error joining route: ' + err.message};
        });
  };

  /**
   * Closes the route specified by |routeId|.
   * @param {!string} routeId
   */
  MediaRouter.prototype.closeRoute = function(routeId) {
    this.handlers_.closeRoute(routeId);
  };

  /**
   * Posts a message to the route designated by |routeId|.
   * @param {!string} routeId
   * @param {!string} message
   * @return {!Promise.<boolean>} Resolved with true if the message was sent,
   *    or false on failure.
   */
  MediaRouter.prototype.sendRouteMessage = function(
      routeId, message) {
    this.handlers_.sendRouteMessage(routeId, message)
        .then(function() {
          return true;
        }, function() {
          return false;
        });
  };

  /**
   * Requests that the provider manager start sending information about active
   * media routes to the Media Router.
   */
  MediaRouter.prototype.startObservingMediaRoutes = function() {
    this.handlers_.startObservingMediaRoutes();
  };

  /**
   * Requests that the provider manager stop sending information about active
   * media routes to the Media Router.
   */
  MediaRouter.prototype.stopObservingMediaRoutes = function() {
    this.handlers_.stopObservingMediaRoutes();
  };

  mediaRouterObserver = new MediaRouterObserver(connector.bindHandleToProxy(
      serviceProvider.connectToService(
          mediaRouterMojom.MediaRouterObserver.name),
      mediaRouterMojom.MediaRouterObserver));

  return mediaRouterObserver;
});

