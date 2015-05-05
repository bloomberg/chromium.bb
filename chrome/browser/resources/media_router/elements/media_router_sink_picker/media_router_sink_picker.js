// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Shows a list of discovered sinks and their routes, if they exist.
Polymer('media-router-sink-picker', {
  publish: {
    /**
     * Maps media_router.Route.id to corresponding media_router.Route.
     *
     * @attribute routeMap
     * @type {?Object<string, !media_router.Route>}
     * @default null
     */
    routeMap: null,

    /**
     * List of discovered sinks.
     *
     * @attribute sinkList
     * @type {?Array<!media_router.Sink>}
     * @default null
     */
    sinkList: null,

    /**
     * Maps media_router.Sink.id to corresponding media_router.Route.id.
     *
     * @attribute sinkToRouteMap
     * @type {?Object<!string, ?string>}
     * @default null
     */
    sinkToRouteMap: null,
  },

  created: function() {
    this.routeMap = {};
    this.sinkList = [];
    this.sinkToRouteMap = {};
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
