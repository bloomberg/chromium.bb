// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Shows information about an available media sink.
Polymer('media-router-sink', {
  publish: {
    /**
     * The sink to show.
     *
     * @attribute sink
     * @type {media_router.Sink}
     * @default {}
     */
    sink: {},

    /**
     * The route to show.
     *
     * @attribute route
     * @type {media_router.Route}
     * @default null
     */
    route: null,
  },
});
