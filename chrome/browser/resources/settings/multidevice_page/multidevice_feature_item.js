// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Item for an individual multidevice feature. These features appear in the
 * multidevice subpage to allow the user to individually toggle them as long as
 * the phone is enabled as a multidevice host. The feature items contain basic
 * information relevant to the individual feature, such as a route to the
 * feature's autonomous page if there is one.
 */
cr.exportPath('settings');

Polymer({
  is: 'settings-multidevice-feature-item',

  properties: {
    /**
     * The localized string representing the name of the feature.
     * @type {string}
     */
    featureName: String,

    /**
     * The localized string providing a description or useful status information
     * concertning the feature.
     * @type {string}
     */
    featureSummaryHtml: String,

    /**
     * The full icon name used provided by the containing iron-iconset-svg
     * (i.e. [iron-iconset-svg name]:[SVG <g> tag id]
     * @type {string}
     */
    iconName: String,

    /**
     * If it is non-null, the item should be actionable and clicking on it
     * should navigate there. If it is undefined, the item is simply not
     * actionable.
     * @type {settings.Route|undefined}
     */
    subpageRoute: Object,
  },

  /**
   * @return {boolean}
   * @private
   */
  hasSubpageClickHandler_: function() {
    return !!this.subpageRoute;
  },

  /** @private */
  onChangeToggle_: function() {
    // TODO (jordynass): Trigger the correct workflow.
    console.log('Toggle changed');
  },

  /** @private */
  handleItemClick_: function(event) {
    if (!this.subpageRoute)
      return;

    // We do not navigate away if the click was on a link.
    if (event.path[0].tagName === 'A')
      return;

    settings.navigateTo(this.subpageRoute);
  },
});
