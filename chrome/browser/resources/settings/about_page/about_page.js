// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-about-page' contains version and OS related
 * information.
 */
Polymer({
  is: 'settings-about-page',

  behaviors: [RoutableBehavior],

  properties: {
    /**
     * The current active route.
     */
    currentRoute: {
      type: Object,
      notify: true,
    },
  },

  /**
   * @type {string} Selector to get the sections.
   * TODO(michaelpg): replace duplicate docs with @override once b/24294625
   * is fixed.
   */
  sectionSelector: 'settings-section',

   /** @override */
  attached: function() {
    this.scroller = this.parentElement;
  },

<if expr="chromeos">
  /** @private */
  onDetailedBuildInfoTap_: function() {
    var animatedPages = /** @type {!SettingsAnimatedPagesElement} */ (
        this.$.pages);
    animatedPages.setSubpageChain(['detailed-build-info']);
  },
</if>
});
