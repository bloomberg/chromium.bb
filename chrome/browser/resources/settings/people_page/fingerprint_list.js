// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

Polymer({
  is: 'settings-fingerprint-list',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /**
     * The list of fingerprint objects.
     * @private {!Array<string>}
     */
    fingerprints_: {
      type: Array,
      value: function() {
        return [];
      }
    }
  },

  /** @private {?settings.FingerprintBrowserProxy} */
  browserProxy_: null,

  /** @override */
  attached: function() {
    this.browserProxy_ = settings.FingerprintBrowserProxyImpl.getInstance();
    this.updateFingerprintsList_();
  },

  /** @private */
  updateFingerprintsList_: function() {
    this.browserProxy_.getFingerprintsList().then(
        this.onFingerprintsChanged_.bind(this));
  },

  /**
   * @param {!settings.FingerprintInfo} fingerprintInfo
   * @private
   * */
  onFingerprintsChanged_: function(fingerprintInfo) {
    this.fingerprints_ = fingerprintInfo.fingerprintsList;
    this.$.fingerprintsList.notifyResize();
    this.$$('.action-button').disabled = fingerprintInfo.isMaxed;
  },

  /**
   * Deletes a fingerprint from |fingerprints_|.
   * @param {!{model: !{index: !number}}} e
   * @private
   */
  onFingerprintDeleteTapped_: function(e) {
    this.browserProxy_.removeEnrollment(e.model.index).then(
        function(success) {
          if (success)
            this.updateFingerprintsList_();
        }.bind(this));
  },

  /**
   * @param {!{model: !{index: !number, item: !string}}} e
   * @private
   */
  onFingerprintLabelChanged_: function(e) {
    this.browserProxy_.changeEnrollmentLabel(e.model.index, e.model.item);
  },

  /**
   * Opens the setup fingerprint dialog.
   * @private
   */
  openAddFingerprintDialog_: function() {
    this.$.setupFingerprint.open();
  },
});
})();
