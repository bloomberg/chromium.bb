// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This Polymer element shows information from media that is currently cast
// to a device. It is assumed that |route| and |sink| correspond to each other.
Polymer({
  is: 'route-details',

  properties: {
    /**
     * The text for the current casting activity status.
     * @private {string}
     */
    activityStatus_: {
      type: String,
      value: '',
    },

    /**
     * The text for the button to switch to the sink picker view.
     * @private {string}
     */
    backToSinkPickerText_: {
      type: String,
      value: function() {
        return loadTimeData.getString('backToSinkPicker');
      },
    },

    /**
     * The route to show.
     * @type {?media_router.Route}
     */
    route: {
      type: Object,
      value: null,
    },

    /**
     * The sink to show.
     * @type {?media_router.Sink}
     */
    sink: {
      type: Object,
      value: null,
      observer: 'updateActivityStatus_',
    },

    /**
     * The text for the stop casting button.
     * @private {string}
     */
    stopCastingButtonText_: {
      type: String,
      value: function() {
        return loadTimeData.getString('stopCastingButton');
      },
    },
  },

  /**
   * Fires a back-click event. This is called when the back link is clicked.
   *
   * @private
   */
  back_: function() {
    this.fire('back-click');
  },

  /**
   * Fires a close-route-click event. This is called when the button to close
   * the current route is clicked.
   *
   * @private
   */
  closeRoute_: function() {
    this.fire('close-route-click', {route: this.route});
  },

  /**
   * Updates |activityStatus_| with the name of |sink|.
   *
   * @private
   */
  updateActivityStatus_: function() {
    this.activityStatus_ = this.sink ?
        loadTimeData.getStringF('castingActivityStatus', this.sink.name) : '';
  }
});
