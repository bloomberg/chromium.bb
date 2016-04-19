// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This Polymer element shows information from media that is currently cast
// to a device.
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
     * Whether the browser is currently incognito.
     * @type {boolean}
     */
    isOffTheRecord: {
      type: Boolean,
      value: false,
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

  behaviors: [
    I18nBehavior,
  ],

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
   * Fires a start-casting-to-route-click event. This is called when the button
   * to start casting to the current route is clicked.
   *
   * @private
   */
  startCastingToRoute_: function() {
    this.fire('start-casting-to-route-click', {route: this.route});
  },

  /**
   * Loads the custom controller if |route.customControllerPath| exists.
   * Falls back to the default route details view otherwise, or if load fails.
   * Updates |activityStatus_| for the default view.
   *
   * @private
   */
  maybeLoadCustomController_: function() {
    this.activityStatus_ = this.route ?
        loadTimeData.getStringF('castingActivityStatus',
                                this.route.description) :
        '';

    if (!this.route || !this.route.customControllerPath ||
        this.isOffTheRecord) {
      this.isCustomControllerHidden_ = true;
      return;
    }

    // Show custom controller
    var extensionview = this.$['custom-controller'];

    // Do nothing if the url is the same and the view is not hidden.
    if (this.route.customControllerPath == extensionview.src &&
        !this.isCustomControllerHidden_)
      return;

    var that = this;
    extensionview.load(this.route.customControllerPath)
    .then(function() {
      // Load was successful; show the custom controller.
      that.isCustomControllerHidden_ = false;
    }, function() {
      // Load was unsuccessful; fall back to default view.
      that.isCustomControllerHidden_ = true;
    });
  },
});
