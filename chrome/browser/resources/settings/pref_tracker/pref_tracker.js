// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * `settings-pref-tracker` is a utility element used to track the
 * initialization of a specified preference and throw an error if the pref
 * is not defined after prefs have all been fetched.
 *
 * Example:
 *
 *    <settings-pref-tracker pref="{{prefs.settings.foo.bar}}">
 *    </settings-pref-tracker>
 *
 * @element settings-pref-tracker
 */
(function() {

  Polymer({
    is: 'settings-pref-tracker',

    properties: {
      /**
       * The Preference object being tracked.
       * @type {?PrefObject}
       */
      pref: {
        type: Object,
        observer: 'validate_',
      },
    },

    /** @override */
    ready: function() {
      this.validate_();
    },

    /**
     * Logs an error once prefs are initialized if the tracked pref is
     * not found.
     * @private
     */
    validate_: function() {
      CrSettingsPrefs.initialized.then(function() {
        // Note that null == undefined.
        if (this.pref == null) {
          // HACK ALERT: This is the best clue we have as to the pref key for
          // this tracker. This value should not be relied upon anywhere or
          // actually used besides for this error message.
          var parentControlHTML = this.domHost && this.domHost.outerHTML;

          console.error('Pref not found. Parent control:' +
              (parentControlHTML || 'Unknown'));
        }
      }.bind(this));
    },
  });
})();
