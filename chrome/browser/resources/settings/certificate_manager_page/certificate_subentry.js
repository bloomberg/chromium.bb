// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview settings-certificate-subentry represents an SSL certificate
 * sub-entry.
 */
Polymer({
  is: 'settings-certificate-subentry',

  properties: {
    /** @type {!CertificateSubnode>} */
    model: Object,

    /** @type {!settings.CertificateType} */
    certificateType: String,
  },

  /** @private {!settings.CertificatesManagerBrowserProxy} */
  browserProxy_: null,

  /** @override */
  ready: function() {
    this.browserProxy_ = settings.CertificatesBrowserProxyImpl.getInstance();
  },

  /**
   * @param {!Event} event
   * @private
   */
  onViewTap_: function(event) {
    this.closePopupMenu_();
    this.browserProxy_.viewCertificate(this.model.id);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onEditTap_: function(event) {
    this.closePopupMenu_();
    // TODO(dpapad): Open edit dialog.
  },

  /**
   * @param {!Event} event
   * @private
   */
  onDeleteTap_: function(event) {
    this.closePopupMenu_();
    // TODO(dpapad): Open delete confirmation dialog.
  },

  /**
   * @param {!Event} event
   * @private
   */
  onExportTap_: function(event) {
    this.closePopupMenu_();
    if (this.certificateType == settings.CertificateType.PERSONAL) {
      // TODO(dpapad): Open password encryption dialog.
    } else {
      this.browserProxy_.exportCertificate(this.model.id);
    }
  },

  /** @private */
  onImportTap_: function() {
    this.closePopupMenu_();
    if (this.certificateType == settings.CertificateType.PERSONAL) {
      // TODO(dpapad): Figure out when to pass true (ChromeOS?).
      this.browserProxy_.importPersonalCertificate(false).then(
          function(showPasswordPrompt) {
            if (showPasswordPrompt) {
              // TODO(dpapad): Show password decryption dialog.
            }
          }.bind(this));
    } else if (this.certificateType == settings.CertificateType.CA) {
      this.browserProxy_.importCaCertificate().then(
          function(certificateName) {
            // TODO(dpapad): Show import dialog.
          }.bind(this));
    } else if (this.certificateType == settings.CertificateType.SERVER) {
      this.browserProxy_.importServerCertificate();
    }
  },

  /**
   * @param {string} certificateType The type of this certificate.
   * @return {boolean} Whether the certificate can be edited.
   * @private
   */
  canEdit_: function(certificateType) {
    return this.certificateType == settings.CertificateType.CA;
  },

  /**
   * @param {string} certificateType The type of this certificate.
   * @return {boolean} Whether a certificate can be imported.
   * @private
   */
  canImport_: function(certificateType) {
    return this.certificateType != settings.CertificateType.OTHER;
  },

  /** @private */
  closePopupMenu_: function() {
    this.$$('iron-dropdown').close();
  },
});
