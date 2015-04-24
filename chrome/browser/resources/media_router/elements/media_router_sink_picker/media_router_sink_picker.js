// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Shows a list of discovered sinks and their routes, if they exist.
Polymer('media-router-sink-picker', {
  publish: {
    /**
     * List of current routes.
     *
     * @attribute routeList
     * @type {!Array<!media_router.Route>}
     * @default []
     */
    routeList: [],

    /**
     * List of discovered sinks.
     *
     * @attribute sinkList
     * @type {!Array<!media_router.Sink>}
     * @default []
     */
    sinkList: [],
  },

  observe: {
    routeList: 'rebuildRouteMaps',
    sinkList: 'rebuildSinkMap',
  },

  created: function() {
    /**
     * Maps media_router.Sink.id to corresponding media_router.Sink.
     * @type {!Object<string, !media_router.Sink>}
     */
    this.sinkMap = {};

    /**
     * Maps media_router.Route.id to corresponding media_router.Route.
     * @type {!Object<string, !media_router.Route>}
     */
    this.routeMap = {};

    /**
     * Maps media_router.Sink.id to corresponding media_router.Route.id.
     * @type {!Object<string, string>}
     */
    this.sinkToRouteMap = {};
  },

  /**
   * Called when routeList is updated. Rebuilds routeMap and sinkToRouteMap.
   */
  rebuildRouteMaps: function() {
    // Reset routeMap and sinkToRouteMap.
    this.routeMap = {};
    this.sinkToRouteMap = {};

    // Rebuild routeMap and sinkToRouteMap.
    this.routeList.forEach(function(route) {
      this.routeMap[route.id] = route;
      this.sinkToRouteMap[route.sinkId] = route.id;
    }, this);
  },

  /**
   * Called when sinkList is updated. Rebuilds sinkMap.
   */
  rebuildSinkMap: function() {
    // Reset sinkMap.
    this.sinkMap = {};

    // Rebuild sinkMap.
    this.sinkList.forEach(function(sink) {
      this.sinkMap[sink.id] = sink;
    }, this);
  },

  /**
   * Adds route to routeList.
   *
   * @param {!media_router.Route} route The route to add.
   */
  addRoute: function(route) {
    // Check if the route already exists or if its associated sink
    // does not exist.
    if (this.routeMap[route.id] || !this.sinkMap[route.sinkId])
      return;

    // If there is an existing route associated with the same sink, its
    // sinkToRouteMap entry will be overwritten with that of the new route,
    // which results in the correct sink to route mapping.
    this.routeList.push(route);
  },

  /**
   * Returns the sink corresponding to sinkId.
   *
   * @param {string} sinkId The id of the media_router.Sink.
   * @return {!media_router.Sink}
   */
  getSink: function(sinkId) {
    return this.sinkMap[sinkId];
  },

  /**
   * Fires a sink-click event. This is called when a media-router-sink
   * is clicked.
   *
   * @param {!Event} event The event object.
   * @param {Object} detail The details of the event.
   * @param {!Element} sender Reference to clicked node.
   */
  onClickSink: function(event, detail, sender) {
    // TODO(imcheng): Indicate route request is in progress.
    // TODO(imcheng): Only allow one request route at a time.
    this.fire('sink-click', {
        sink: sender.sink,
        route: sender.route
    });
  },
});
