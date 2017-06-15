// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This Polymer element shows the custom controller for a route using
// extensionview.
Polymer({
  is: 'extension-view-wrapper',

  properties: {
    /**
     * Whether the extension view is ready to be shown.
     * @type {boolean}
     */
    isExtensionViewReady: {
      type: Boolean,
      value: false,
      notify: true,
    },

    /**
     * The route to show the custom controller for.
     * @type {?media_router.Route|undefined}
     */
    route: {
      type: Object,
      observer: 'maybeLoadExtensionView_',
    },

    /**
     * The timestamp for when the route details view was opened.
     * @type {number}
     */
    routeDetailsOpenTime: {
      type: Number,
      value: 0,
    },
  },

  /**
   * @return {?string}
   */
  getCustomControllerPath_: function() {
    if (!this.route || !this.route.customControllerPath) {
      return null;
    }
    return this.route.customControllerPath +
        '&requestTimestamp=' + this.routeDetailsOpenTime;
  },

  /**
   * Loads the custom controller if the controller path for the current route is
   * valid.
   */
  maybeLoadExtensionView_: function() {
    /** @const */ var extensionview = this.$['custom-controller'];
    /** @const */ var controllerPath = this.getCustomControllerPath_();

    // Do nothing if the controller path doesn't exist or is already shown in
    // the extension view.
    if (!controllerPath || controllerPath == extensionview.src) {
      return;
    }

    /** @const */ var that = this;
    extensionview.load(controllerPath)
        .then(
            function() {
              // Load was successful; show the custom controller.
              that.isExtensionViewReady = true;
            },
            function() {
              // Load was unsuccessful; fall back to default view.
              that.isExtensionViewReady = false;
            });
  },
});
