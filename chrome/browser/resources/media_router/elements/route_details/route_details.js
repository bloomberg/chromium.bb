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
     * The route to show.
     * @type {?media_router.Route}
     */
    route: {
      type: Object,
      value: null,
      observer: 'maybeLoadCustomController_',
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
     * The ID of the media route provider extension.
     * @type {string}
     */
    routeProviderExtensionId: {
      type: String,
      value: '',
    },

    /**
     * The text for the stop casting button.
     * @private {string}
     */
    stopCastingButtonText_: {
      type: String,
      value: loadTimeData.getString('stopCastingButton'),
    },

    /**
     * Whether the custom controller should be hidden.
     * A custom controller is shown iff |route| specifies customControllerPath
     * and the view can be loaded.
     * @private {boolean}
     */
    isCustomControllerHidden_: {
      type: Boolean,
      value: true,
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
   * Loads the custom controller if |route.customControllerPath| exists.
   * Falls back to the default route details view otherwise, or if load fails.
   *
   * @private
   */
  maybeLoadCustomController_: function() {
    if (!this.route || !this.route.customControllerPath ||
        !this.routeProviderExtensionId) {
      this.isCustomControllerHidden_ = true;
      return;
    }

    // Show custom controller
    var fullUrl = 'chrome-extension://' + this.routeProviderExtensionId + '/' +
        this.route.customControllerPath;
    var extensionview = this.$['custom-controller'];
    if (fullUrl == extensionview.src && !this.isCustomControllerHidden_) {
      // Do nothing if the url is the same and the view is not hidden.
      return;
    }

    this.isCustomControllerHidden_ = false;
    var that = this;
    extensionview.load(fullUrl).catch(function() {
      // Fall back to default view.
      that.isCustomControllerHidden_ = true;
    });
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
