// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const OptionsPage = options.OptionsPage;

  /**
   * CertificateEditCaTrustOverlay class
   * Encapsulated handling of the 'enter Edit password' overlay page.
   * @class
   */
  function CertificateEditCaTrustOverlay() {
    OptionsPage.call(this, 'certificateEditCaTrustOverlay',
                     '',
                     'certificateEditCaTrustOverlay');
  }

  cr.addSingletonGetter(CertificateEditCaTrustOverlay);

  CertificateEditCaTrustOverlay.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * Initializes the page.
     */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      var self = this;
      $('certificateEditCaTrustCancelButton').onclick = function(event) {
        self.cancelEdit_();
      }
      $('certificateEditCaTrustOkButton').onclick = function(event) {
        self.finishEdit_();
      }
    },

    /**
     * Dismisses the overlay.
     * @private
     */
    dismissOverlay_: function() {
      OptionsPage.clearOverlays();
    },

    /**
     * Enables or disables input fields.
     * @private
     */
    enableInputs_: function(enabled) {
      $('certificateCaTrustSSLCheckbox').disabled =
      $('certificateCaTrustEmailCheckbox').disabled =
      $('certificateCaTrustObjSignCheckbox').disabled =
      $('certificateEditCaTrustCancelButton').disabled =
      $('certificateEditCaTrustOkButton').disabled = !enabled;
    },


    /**
     * Attempt the Edit operation.
     * The overlay will be left up with inputs disabled until the backend
     * finishes and dismisses it.
     * @private
     */
    finishEdit_: function() {
      // TODO(mattm): Send checked values as booleans.  For now send them as
      // strings, since DOMUIBindings::send does not support any other types :(
      chrome.send('editCaCertificateTrust',
                  [this.certId,
                   $('certificateCaTrustSSLCheckbox').checked.toString(),
                   $('certificateCaTrustEmailCheckbox').checked.toString(),
                   $('certificateCaTrustObjSignCheckbox').checked.toString()]);
      this.enableInputs_(false);
    },

    /**
     * Cancel the Edit operation.
     * @private
     */
    cancelEdit_: function() {
      this.dismissOverlay_();
    },
  };

  /**
   * Callback from CertificateManagerHandler with the trust values.
   * @param {boolean} trustSSL The initial value of SSL trust checkbox.
   * @param {boolean} trustEmail The initial value of Email trust checkbox.
   * @param {boolean} trustObjSign The initial value of Object Signing trust
   */
  CertificateEditCaTrustOverlay.populateTrust = function(
      trustSSL, trustEmail, trustObjSign) {
    $('certificateCaTrustSSLCheckbox').checked = trustSSL;
    $('certificateCaTrustEmailCheckbox').checked = trustEmail;
    $('certificateCaTrustObjSignCheckbox').checked = trustObjSign;
    CertificateEditCaTrustOverlay.getInstance().enableInputs_(true);
  }

  /**
   * Show the Edit CA Trust overlay.
   * @param {string} certId The id of the certificate to be passed to the
   * certificate manager model.
   * @param {string} certName The display name of the certificate.
   * checkbox.
   */
  CertificateEditCaTrustOverlay.show = function(certId, certName) {
    CertificateEditCaTrustOverlay.getInstance().certId = certId;
    $('certificateEditCaTrustDescription').textContent =
        localStrings.getStringF('certificateEditCaTrustDescriptionFormat',
                                certName);
    CertificateEditCaTrustOverlay.getInstance().enableInputs_(false);
    OptionsPage.showOverlay('certificateEditCaTrustOverlay');
    chrome.send('getCaCertificateTrust', [certId]);
  }

  CertificateEditCaTrustOverlay.dismiss = function() {
    CertificateEditCaTrustOverlay.getInstance().dismissOverlay_();
  };

  // Export
  return {
    CertificateEditCaTrustOverlay: CertificateEditCaTrustOverlay
  };

});
