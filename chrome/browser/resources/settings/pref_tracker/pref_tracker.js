// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * `cr-settings-pref-tracker` is a utility element used to track the
 * initialization of a specified preference and throw an error if the pref
 * is not defined after prefs have all been fetched.
 *
 * Example:
 *
 *    <cr-settings-pref-tracker pref="{{prefs.settings.foo.bar}}">
 *    </cr-settings-pref-tracker>
 *
 * @element cr-settings-pref-tracker
 */
(function() {

  /**
   * An array of all the tracker instances.
   * @type {!Array<!CrSettingsPrefTrackerElement>}
   */
  var instances = [];

  /**
   * Validates all tracker instances.
   * @private
   */
  var validateAll_ = function() {
    instances.forEach(function(tracker) {
      tracker.validate_();
    });
  };

  document.addEventListener(CrSettingsPrefs.INITIALIZED, validateAll_);

  Polymer({
    is: 'cr-settings-pref-tracker',

    properties: {
      /**
       * The Preference object being tracked.
       * @type {?chrome.settingsPrivate.PrefObject}
       */
      pref: {
        type: Object,
        observer: 'validate_',
      },
    },

    /** @override */
    ready: function() {
      this.validate_();

      instances.push(this);
    },

    /**
     * Throws an error if prefs are initialized and the tracked pref is not
     * found.
     * @private
     */
    validate_: function() {
      this.async(function() {
        // Note that null == undefined.
        if (CrSettingsPrefs.isInitialized && this.pref == null) {
          // HACK ALERT: This is the best clue we have as to the pref key for
          // this tracker. This value should not be relied upon anywhere or
          // actually used besides for this error message.
          var keyHint = '';
          var parentPrefString = this.parentNode && this.parentNode.host &&
              this.parentNode.host.getAttribute('pref');
          if (parentPrefString) {
            keyHint = parentPrefString.match(/{{([a-z0-9._]+)}}/)[1];
          }

          throw new Error('Pref not found. Key Hint: ' + keyHint);
        }
      });
    },
  });
})();
