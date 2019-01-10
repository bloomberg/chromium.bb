// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-a11y-page' is the small section of advanced settings with
 * a link to the web store accessibility page on most platforms, and
 * a subpage with lots of other settings on Chrome OS.
 */
Polymer({
  is: 'settings-a11y-page',

  properties: {
    /**
     * The current active route.
     */
    currentRoute: {
      type: Object,
      notify: true,
    },

    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value: function() {
        const map = new Map();
        // <if expr="chromeos">
        if (settings.routes.MANAGE_ACCESSIBILITY) {
          map.set(
              settings.routes.MANAGE_ACCESSIBILITY.path, '#subpage-trigger');
        }
        // </if>
        return map;
      },
    },

    /**
     * Whether to show experimental accessibility label features.
     * @private {boolean}
     */
    showExperimentalAccessibilityLabels_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('showExperimentalA11yLabels');
      },
    },

    // <if expr="chromeos">
    /**
     * Whether to show experimental accessibility features.
     * Only used in Chrome OS.
     * @private {boolean}
     */
    showExperimentalFeatures_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('showExperimentalA11yFeatures');
      },
    },
    // </if>
  },

  // <if expr="chromeos">
  /** @private */
  onManageAccessibilityFeaturesTap_: function() {
    settings.navigateTo(settings.routes.MANAGE_ACCESSIBILITY);
  },
  // </if>

  /** private */
  onMoreFeaturesLinkClick_: function() {
    window.open(
        'https://chrome.google.com/webstore/category/collection/accessibility');
  },
});
