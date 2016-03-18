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
   * @extends {settings.TestBrowserProxy}
   */
  var TestCertificatesBrowserProxy = function() {
    settings.TestBrowserProxy.call(this, [
      'deleteCertificate',
      'editCaCertificateTrust',
      'exportPersonalCertificatePasswordSelected',
      'getCaCertificateTrust',
      'importPersonalCertificatePasswordSelected',
    ]);

    /** @private {!CaTrustInfo} */
    this.caTrustInfo_ = {ssl: true, email: true, objSign: true};
  };

  TestCertificatesBrowserProxy.prototype = {
    __proto__: settings.TestBrowserProxy.prototype,

    /**
     * @param {!CaTrustInfo} caTrustInfo
     */
    setCaCertificateTrust: function(caTrustInfo) {
      this.caTrustInfo_ = caTrustInfo;
    },

    /** @override */
    getCaCertificateTrust: function(id) {
      this.methodCalled('getCaCertificateTrust', id);
      return Promise.resolve(this.caTrustInfo_);
    },

    /** @override */
    editCaCertificateTrust: function(id, ssl, email, objSign) {
      this.methodCalled('editCaCertificateTrust', {
        id: id, ssl: ssl, email: email, objSign: objSign,
      });
      return Promise.resolve();
    },

    /** @override */
    deleteCertificate: function(id) {
      this.methodCalled('deleteCertificate', id);
      return Promise.resolve();
    },

    /** @override */
    exportPersonalCertificatePasswordSelected: function(password) {
      this.resolverMap_.get(
          'exportPersonalCertificatePasswordSelected').resolve(password);
      return Promise.resolve();
    },

    /** @override */
    importPersonalCertificatePasswordSelected: function(password) {
      this.resolverMap_.get(
          'importPersonalCertificatePasswordSelected').resolve(password);
      return Promise.resolve();
    },
  };

  /** @return {!CertificateSubnode} */
  function createSampleCertificateSubnode() {
    return {
      extractable: false,
      id: 'dummyId',
      name: 'dummyName',
      policy: false,
      readonly: false,
      untrusted: false,
      urlLocked: false,
    };
  }

  /**
   * Triggers an 'input' event on the given text input field (which triggers
   * validation to occur for password fields being tested in this file).
   * @param {!PaperInputElement} element
   */
  function triggerInputEvent(element) {
    // The actual key code is irrelevant for tests.
    var kSpaceBar = 32;
    MockInteractions.keyEventOn(element, 'input', kSpaceBar);
  }

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

  function registerPasswordEncryptDialogTests() {
    /** @type {?SettingsCertificatePasswordEncryptionDialogElement} */
    var dialog = null;

    /** @type {?TestCertificatesBrowserProxy} */
    var browserProxy = null;

    /** @type {!CertificateSubnode} */
    var model = createSampleCertificateSubnode();

    suite('CertificatePasswordEncryptionDialogTests', function() {
      setup(function() {
        browserProxy = new TestCertificatesBrowserProxy();
        settings.CertificatesBrowserProxyImpl.instance_ = browserProxy;
        PolymerTest.clearBody();
        dialog = document.createElement(
            'settings-certificate-password-encryption-dialog');
        dialog.model = model;
        document.body.appendChild(dialog);
      });

      teardown(function() { dialog.remove(); });

      test('EncryptSuccess', function() {
        var passwordInputElements =
            Polymer.dom(dialog.$.dialog).querySelectorAll('paper-input');
        var passwordInputElement = passwordInputElements[0];
        var confirmPasswordInputElement = passwordInputElements[1];

        assertTrue(dialog.$.dialog.opened);
        assertTrue(dialog.$.ok.disabled);

        // Test that the 'OK' button is disabled when the password fields are
        // empty (even though they both have the same value).
        triggerInputEvent(passwordInputElement);
        assertTrue(dialog.$.ok.disabled);

        // Test that the 'OK' button is disabled until the two password fields
        // match.
        passwordInputElement.value = 'foopassword';
        triggerInputEvent(passwordInputElement);
        assertTrue(dialog.$.ok.disabled);
        confirmPasswordInputElement.value = passwordInputElement.value;
        triggerInputEvent(confirmPasswordInputElement);
        assertFalse(dialog.$.ok.disabled);

        // Simulate clicking 'OK'.
        MockInteractions.tap(dialog.$.ok);

        var methodName = 'exportPersonalCertificatePasswordSelected';
        return browserProxy.whenCalled(methodName).then(function(password) {
          assertEquals(passwordInputElement.value, password);
          // Check that the dialog is closed.
          assertFalse(dialog.$.dialog.opened);
        });
      });
    });
  }

  function registerPasswordDecryptDialogTests() {
    /** @type {?SettingsCertificatePasswordDecryptionDialogElement} */
    var dialog = null;

    /** @type {?TestCertificatesBrowserProxy} */
    var browserProxy = null;

    suite('CertificatePasswordDecryptionDialogTests', function() {
      setup(function() {
        browserProxy = new TestCertificatesBrowserProxy();
        settings.CertificatesBrowserProxyImpl.instance_ = browserProxy;
        PolymerTest.clearBody();
        dialog = document.createElement(
            'settings-certificate-password-decryption-dialog');
        document.body.appendChild(dialog);
      });

      teardown(function() { dialog.remove(); });

      test('DecryptSuccess', function() {
        var passwordInputElement =
            Polymer.dom(dialog.$.dialog).querySelector('paper-input');
        assertTrue(dialog.$.dialog.opened);
        assertTrue(dialog.$.ok.disabled);

        // Test that the 'OK' button is disabled when the password field is
        // empty.
        triggerInputEvent(passwordInputElement);
        assertTrue(dialog.$.ok.disabled);
        passwordInputElement.value = 'foopassword';
        triggerInputEvent(passwordInputElement);
        assertFalse(dialog.$.ok.disabled);

        // Simulate clicking 'OK'.
        MockInteractions.tap(dialog.$.ok);

        var methodName = 'importPersonalCertificatePasswordSelected';
        return browserProxy.whenCalled(methodName).then(function(password) {
          assertEquals(passwordInputElement.value, password);
          // Check that the dialog is closed.
          assertFalse(dialog.$.dialog.opened);
        });
      });
    });
  }

  return {
    registerTests: function() {
      registerCaTrustEditDialogTests();
      registerDeleteDialogTests();
      registerPasswordEncryptDialogTests();
      registerPasswordDecryptDialogTests();
    },
  };
});
