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

  behaviors: [WebUIListenerBehavior],

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

    /**
     * Whether to show accessibility labels settings.
     */
    showAccessibilityLabelsSetting_: {
      type: Boolean,
      value: false,
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

  /** @override */
  ready: function() {
    this.addWebUIListener(
        'screen-reader-state-changed',
        this.onScreenReaderStateChanged_.bind(this));
    chrome.send('getScreenReaderState');
  },

  /**
   * @private
   * @param {boolean} hasScreenReader Whether a screen reader is enabled.
   */
  onScreenReaderStateChanged_: function(hasScreenReader) {
    // TODO(katie): Remove showExperimentalA11yLabels flag before launch.
    this.showAccessibilityLabelsSetting_ = hasScreenReader &&
        loadTimeData.getBoolean('showExperimentalA11yLabels');
  },

  /** @private */
  onToggleAccessibilityImageLabels_: function() {
    const a11yImageLabelsOn = this.$.a11yImageLabels.checked;
    if (a11yImageLabelsOn) {
      chrome.send('confirmA11yImageLabels');
    }
    chrome.metricsPrivate.recordBoolean(
        'Accessibility.ImageLabels.FromSettings.ToggleSetting',
        a11yImageLabelsOn);
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
