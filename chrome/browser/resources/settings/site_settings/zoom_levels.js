// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'zoom-levels' is the polymer element for showing the sites that are zoomed in
 * or out.
 */

Polymer({
  is: 'zoom-levels',

  behaviors: [SiteSettingsBehavior, WebUIListenerBehavior],

  properties: {
    /**
     * Array of sites that are zoomed in or out.
     * @type {!Array<ZoomLevelEntry>}
     */
    sites_: Array,
  },

  /** @override */
  ready: function() {
    this.addWebUIListener('onZoomLevelsChanged',
        this.onZoomLevelsChanged_.bind(this));
    this.browserProxy.fetchZoomLevels();
  },

  /**
   * A handler for when zoom levels change.
   * @param {!Array<ZoomLevelEntry>} sites The up to date list of sites and
   *     their zoom levels.
   */
  onZoomLevelsChanged_: function(sites) {
    this.sites_ = sites;
  },

  /**
   * A handler for when a zoom level for a site is deleted.
   * @param {!{model: !{index: number}}} event
   * @private
   */
  removeZoomLevel_: function(event) {
    var site = this.sites_[event.model.index];
    this.browserProxy.removeZoomLevel(site.origin);
  },
});
