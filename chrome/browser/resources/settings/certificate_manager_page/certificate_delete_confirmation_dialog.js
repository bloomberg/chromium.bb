// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A confirmation dialog allowing the user to delete various types
 * of certificates.
 */
Polymer({
  is: 'settings-certificate-delete-confirmation-dialog',

  properties: {
    /** @private {!settings.CertificatesBrowserProxy} */
    browserProxy_: Object,

    /** @type {!CertificateSubnode} */
    model: Object,

    /** @type {!settings.CertificateType} */
    certificateType: String,
  },

  /** @override */
  ready: function() {
    this.browserProxy_ = settings.CertificatesBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached: function() {
    this.$.dialog.open();
  },

  /**
   * @private
   * @return {string}
   */
  getTitleText_: function() {
    /**
     * @param {string} localizedMessageId
     * @return {string}
     */
    var getString = function(localizedMessageId) {
      return loadTimeData.getStringF(localizedMessageId, this.model.name);
    }.bind(this);

    switch (this.certificateType) {
      case settings.CertificateType.PERSONAL:
        return getString('certificateManagerDeleteUserTitle');
      case settings.CertificateType.SERVER:
        return getString('certificateManagerDeleteServerTitle');
      case settings.CertificateType.CA:
        return getString('certificateManagerDeleteCaTitle');
      case settings.CertificateType.OTHER:
        return getString('certificateManagerDeleteOtherTitle');
    }
    assertNotReached();
  },

  /**
   * @private
   * @return {string}
   */
  getDescriptionText_: function() {
    var getString = loadTimeData.getString.bind(loadTimeData);
    switch (this.certificateType) {
      case settings.CertificateType.PERSONAL:
        return getString('certificateManagerDeleteUserDescription');
      case settings.CertificateType.SERVER:
        return getString('certificateManagerDeleteServerDescription');
      case settings.CertificateType.CA:
        return getString('certificateManagerDeleteCaDescription');
      case settings.CertificateType.OTHER:
        return '';
    }
    assertNotReached();
  },

  /** @private */
  onCancelTap_: function() {
    this.$.dialog.close();
  },

  /** @private */
  onOkTap_: function() {
    this.browserProxy_.deleteCertificate(this.model.id).then(
        function() {
          this.$.dialog.close();
        }.bind(this),
        /** @param {!CertificatesError} error */
        function(error) {
          this.$.dialog.close();
          this.fire('certificates-error', error);
        }.bind(this));
  },
});
