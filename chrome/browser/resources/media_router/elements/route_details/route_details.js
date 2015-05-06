// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This Polymer element shows information from media that is currently cast
// to a device. It is assumed that |route| and |sink| correspond to each other.
Polymer('route-details', {
  publish: {
    /**
     * The route to show.
     *
     * @attribute route
     * @type {media_router.Route}
     * @default null
     */
    route: null,

    /**
     * The sink to show.
     *
     * @attribute sink
     * @type {media_router.Sink}
     * @default null
     */
    sink: null,
  },

  observe: {
    sink: 'updateActivityStatus',
  },

  /**
   * The current casting activity status.
   * @private {string}
   * @default ''
   */
  activityStatus_: '',

  /**
   * Fires a back-click event. This is called when the back link is clicked.
   */
  back: function() {
    this.fire('back-click');
  },

  /**
   * Fires a close-route-click event. This is called when the button to close
   * the current route is clicked.
   */
  closeRoute: function() {
    this.fire('close-route-click', {route: this.route});
  },

  /**
   * Updates |activityStatus_| with the name of |sink|.
   */
  updateActivityStatus: function() {
    this.activityStatus_ = this.sink ?
        loadTimeData.getStringF('castingActivityStatus', this.sink.name) : '';
  }
});
