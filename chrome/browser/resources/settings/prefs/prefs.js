/* Copyright 2015 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

/**
 * @fileoverview
 * 'cr-settings-prefs' is an element which serves as a model for
 * interaction with settings which are stored in Chrome's
 * Preferences.
 *
 * Example:
 *
 *    <cr-settings-prefs id="prefs"></cr-settings-prefs>
 *    <cr-settings-a11y-page prefs="{{this.$.prefs}}"></cr-settings-a11y-page>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-a11y-page
 */
Polymer('cr-settings-prefs', {
  publish: {
    /**
     * Accessibility preferences state.
     *
     * @attribute settings
     * @type CrSettingsPrefs.Settings
     * @default null
     */
    settings: null,
  },

  /** @override */
  created: function() {
    'use strict';

    this.settings = {};
    this.initializeA11y_();
    this.initializeDownloads_();
    var observer = new ObjectObserver(this.settings);
    observer.open(this.propertyChangeCallback_.bind(this, 'settings'));

    // For all Object properties of settings, create an ObjectObserver.
    for (let key in this.settings) {
      if (typeof this.settings[key] == 'object') {
        let keyObserver = new ObjectObserver(this.settings[key]);
        keyObserver.open(
            this.propertyChangeCallback_.bind(this, 'settings.' + key));
      }
    }
  },

  /**
   * Initializes some defaults for the a11y settings.
   * @private
   */
  initializeA11y_: function() {
    this.settings.a11y = {
      enableMenu: true,
      largeCursorEnabled: false,
      highContrastEnabled: false,
      stickyKeysEnabled: false,
      screenMagnifier: false,
      enableTapDragging: false,
      autoclick: false,
      autoclickDelayMs: 200,
      virtualKeyboard: false,
    };

    this.settings.touchpad = {
      enableTapDragging: false,
    };

    // ChromeVox is enbaled/disabled via the 'settings.accessibility' boolean
    // pref.
    this.settings.accessibility = false;

    // TODO(jlklein): Actually pull the data out of prefs and initialize.
  },

  /**
   * Initializes some defaults for the downloads settings.
   * @private
   */
  initializeDownloads_: function() {
    this.settings.download = {
      downloadLocation: '',
      promptForDownload: false,
    };
  },

  /**
   * @param {string} propertyPath The path before the property names.
   * @param {!Array<string>} added An array of keys which were added.
   * @param {!Array<string>} removed An array of keys which were removed.
   * @param {!Array<string>} changed An array of keys of properties whose
   *     values changed.
   * @param {function(string) : *} getOldValueFn A function which takes a
   *     property name and returns the old value for that property.
   * @private
   */
  propertyChangeCallback_: function(
      propertyPath, added, removed, changed, getOldValueFn) {
    Object.keys(changed).forEach(function(property) {
      console.log(
          `${propertyPath}.${property}`,
          `old : ${getOldValueFn(property)}`,
          `newValue : ${changed[property]}`);

      // TODO(jlklein): Actually set the changed property back to prefs.
    });
   },
});
