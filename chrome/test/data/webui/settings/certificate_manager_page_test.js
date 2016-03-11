// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('certificate_manager_page', function() {
  /**
   * A test version of CertificatesBrowserProxy. Provides helper methods
   * for allowing tests to know when a method was called, as well as
   * specifying mock responses.
   *
   * @constructor
   * @implements {settings.CertificatesBrowserProxy}
   */
  var TestCertificatesBrowserProxy = function() {
    /** @private {!Map<string, !PromiseResolver>} */
    this.resolverMap_ = new Map();
    var wrapperMethods = [
      'deleteCertificate',
      'editCaCertificateTrust',
      'getCaCertificateTrust',
    ];
    wrapperMethods.forEach(this.resetResolver, this);

    /** @private {!CaTrustInfo} */
    this.caTrustInfo_ = {ssl: true, email: true, objSign: true};
  };

  TestCertificatesBrowserProxy.prototype = {
    /**
     * @param {string} methodName
     * @return {!Promise} A promise that is resolved when the given method
     *     is called.
     */
    whenCalled: function(methodName) {
      return this.resolverMap_.get(methodName).promise;
    },

    /**
     * Resets the PromiseResolver associated with the given method.
     * @param {string} methodName
     */
    resetResolver: function(methodName) {
      this.resolverMap_.set(methodName, new PromiseResolver());
    },

    /**
     * @param {!CaTrustInfo} caTrustInfo
     */
    setCaCertificateTrust: function(caTrustInfo) {
      this.caTrustInfo_ = caTrustInfo;
    },

    /** @override */
    getCaCertificateTrust: function(id) {
      this.resolverMap_.get('getCaCertificateTrust').resolve(id);
      return Promise.resolve(this.caTrustInfo_);
    },

    /** @override */
    editCaCertificateTrust: function(id, ssl, email, objSign) {
      this.resolverMap_.get('editCaCertificateTrust').resolve({
        id: id, ssl: ssl, email: email, objSign: objSign,
      });
      return Promise.resolve();
    },

    /** @override */
    deleteCertificate: function(id) {
      this.resolverMap_.get('deleteCertificate').resolve(id);
      return Promise.resolve();
    },
  };

  /** @return {!CertificateSubnode} */
  var createSampleCertificateSubnode = function() {
    return {
      extractable: false,
      id: 'dummyId',
      name: 'dummyName',
      policy: false,
      readonly: false,
      untrusted: false,
      urlLocked: false,
    };
  };

  function registerCaTrustEditDialogTests() {
    /** @type {?SettingsCaTrustEditDialogElement} */
    var dialog = null;

    /** @type {?TestCertificatesBrowserProxy} */
    var browserProxy = null;

    /** @type {!CertificateSubnode} */
    var model = createSampleCertificateSubnode();

    /** @type {!CaTrustInfo} */
    var caTrustInfo = { ssl: true, email: false, objSign: false };

    suite('CaTrustEditDialogTests', function() {
      setup(function() {
        browserProxy = new TestCertificatesBrowserProxy();
        browserProxy.setCaCertificateTrust(caTrustInfo);

        settings.CertificatesBrowserProxyImpl.instance_ = browserProxy;
        PolymerTest.clearBody();
        dialog = document.createElement('settings-ca-trust-edit-dialog');
        dialog.model = model;
        document.body.appendChild(dialog);
      });

      teardown(function() { dialog.remove(); });

      test('Dialog', function() {
        return browserProxy.whenCalled('getCaCertificateTrust').then(
            function(id) {
              assertEquals(model.id, id);
              assertEquals(caTrustInfo.ssl, dialog.$.ssl.checked);
              assertEquals(caTrustInfo.email, dialog.$.email.checked);
              assertEquals(caTrustInfo.objSign, dialog.$.objSign.checked);

              // Simulate toggling all checkboxes.
              MockInteractions.tap(dialog.$.ssl);
              MockInteractions.tap(dialog.$.email);
              MockInteractions.tap(dialog.$.objSign);

              // Simulate clicking 'OK'.
              MockInteractions.tap(dialog.$.ok);

              return browserProxy.whenCalled('editCaCertificateTrust').then(
                  function(args) {
                    assertEquals(model.id, args.id);
                    // Checking that the values sent to C++ are reflecting the
                    // changes made by the user (toggling all checkboxes).
                    assertEquals(caTrustInfo.ssl, !args.ssl);
                    assertEquals(caTrustInfo.email, !args.email);
                    assertEquals(caTrustInfo.objSign, !args.objSign);
                    // Check that the dialog is closed.
                    assertFalse(dialog.$.dialog.opened);
                  });
            });
      });
    });
  }

  function registerDeleteDialogTests() {
    /** @type {?SettingsCertificateDeleteConfirmationDialogElement} */
    var dialog = null;

    /** @type {?TestCertificatesBrowserProxy} */
    var browserProxy = null;

    /** @type {!CertificateSubnode} */
    var model = createSampleCertificateSubnode();

    suite('CertificateDeleteConfirmationDialogTests', function() {
      setup(function() {
        browserProxy = new TestCertificatesBrowserProxy();
        settings.CertificatesBrowserProxyImpl.instance_ = browserProxy;
        PolymerTest.clearBody();
        dialog = document.createElement(
            'settings-certificate-delete-confirmation-dialog');
        dialog.model = model;
        dialog.certificateType = settings.CertificateType.PERSONAL;
        document.body.appendChild(dialog);
      });

      teardown(function() { dialog.remove(); });

      test('DeleteSuccess', function() {
        assertTrue(dialog.$.dialog.opened);
        // Check that the dialog title includes the certificate name.
        var titleEl = Polymer.dom(dialog.$.dialog).querySelector('.title');
        assertTrue(titleEl.textContent.includes(model.name));

        // Simulate clicking 'OK'.
        MockInteractions.tap(dialog.$.ok);

        return browserProxy.whenCalled('deleteCertificate').then(
            function(id) {
              assertEquals(model.id, id);
              // Check that the dialog is closed.
              assertFalse(dialog.$.dialog.opened);
            });
      });
    });
  }

  return {
    registerCaTrustEditDialogTests: registerCaTrustEditDialogTests,
    registerDeleteDialogTests: registerDeleteDialogTests,
  };
});
