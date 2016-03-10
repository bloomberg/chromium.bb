// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-ca-trust-edit-dialog' is the a dialog allowing the user to edit the
 * trust lever of a given certificate authority.
 *
 * @group Chrome Settings Elements
 * @element settings-ca-trust-edit-dialog
 */
Polymer({
  is: 'settings-ca-trust-edit-dialog',

  properties: {
    /** @private {!settings.CertificatesBrowserProxy} */
    browserProxy_: Object,

    /** @type {!CertificateSubnode} */
    model: Object,

    /** @private {?CaTrustInfo} */
    trustInfo_: Object,

    /** @private {string} */
    explanationText_: String,
  },

  /** @override */
  ready: function() {
    this.browserProxy_ = settings.CertificatesBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached: function() {
    this.explanationText_ = loadTimeData.getStringF(
        'certificateManagerCaTrustEditDialogExplanation',
        this.model.name);
    this.browserProxy_.getCaCertificateTrust(this.model.id).then(
        /** @param {!CaTrustInfo} trustInfo */
        function(trustInfo) {
          this.trustInfo_ = trustInfo;
          this.$.dialog.open();
        }.bind(this));
  },

  /** @private */
  onCancelTap_: function() {
    this.$.dialog.close();
  },

  /** @private */
  onOkTap_: function() {
    this.$.spinner.active = true;
    this.browserProxy_.editCaCertificateTrust(
        this.model.id, this.$.ssl.checked,
        this.$.email.checked, this.$.objSign.checked).then(function() {
      this.$.spinner.active = false;
      this.$.dialog.close();
    }.bind(this),
    /** @param {!CertificatesError} error */
    function(error) {
      // TODO(dpapad): Display error here.
    });
  },
});
