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

  behaviors: [I18nBehavior],

  properties: {
    /**
     * This is the ID of the localized string representing the name of the
     * feature.
     * @type {string}
     */
    featureNameId: String,

    /**
     * This is the ID of the localized string providing a description or useful
     * status information concertning the feature.
     * @type {string}
     */
    featureSummaryId: String,

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

  /** @private @return {string} */
  getFeatureName_: function() {
    return this.i18nAdvanced(this.featureNameId);
  },

  /** @private @return {string} */
  getSubLabelInnerHtml_: function() {
    return this.i18nAdvanced(this.featureSummaryId);
  },

  /** @private @return {boolean} */
  hasSubpageClickHandler_: function() {
    return !!this.subpageRoute;
  },

  onChangeToggle_: function() {
    // TODO (jordynass): Trigger the correct workflow.
    console.log('Toggle changed');
  },

  /** @private */
  handleItemClick_: function() {
    if (!this.subpageRoute)
      return;
    settings.navigateTo(this.subpageRoute);
  }
});
