// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These test suites test the certificate-manager shared component in the
// context of the Settings privacy page. This simplifies the test setup and
// provides better context for testing.

cr.define('certificate_manager', function() {
  /**
   * A test version of CertificatesBrowserProxy. Provides helper methods
   * for allowing tests to know when a method was called, as well as
   * specifying mock responses.
   *
   * @implements {certificate_manager.CertificatesBrowserProxy}
   */
  class TestCertificatesBrowserProxy extends TestBrowserProxy {
    constructor() {
      super([
        'deleteCertificate',
        'editCaCertificateTrust',
        'exportCertificate',
        'exportPersonalCertificate',
        'exportPersonalCertificatePasswordSelected',
        'getCaCertificateTrust',
        'importCaCertificate',
        'importCaCertificateTrustSelected',
        'importPersonalCertificate',
        'importPersonalCertificatePasswordSelected',
        'importServerCertificate',
        'refreshCertificates',
        'viewCertificate',
      ]);

      /** @private {!CaTrustInfo} */
      this.caTrustInfo_ = {ssl: true, email: true, objSign: true};

      /** @private {?CertificatesError} */
      this.certificatesError_ = null;
    }

    /** @param {!CaTrustInfo} caTrustInfo */
    setCaCertificateTrust(caTrustInfo) {
      this.caTrustInfo_ = caTrustInfo;
    }

    /** @override */
    getCaCertificateTrust(id) {
      this.methodCalled('getCaCertificateTrust', id);
      return Promise.resolve(this.caTrustInfo_);
    }

    /** @override */
    importServerCertificate() {
      this.methodCalled('importServerCertificate');
      return Promise.resolve();
    }

    /** @override */
    importCaCertificate() {
      this.methodCalled('importCaCertificate');
      return Promise.resolve('dummyName');
    }

    /** @override */
    importCaCertificateTrustSelected(ssl, email, objSign) {
      this.methodCalled(
          'importCaCertificateTrustSelected',
          {ssl: ssl, email: email, objSign: objSign});
      return this.fulfillRequest_();
    }

    /** @override */
    editCaCertificateTrust(id, ssl, email, objSign) {
      this.methodCalled(
          'editCaCertificateTrust',
          {id: id, ssl: ssl, email: email, objSign: objSign});
      return this.fulfillRequest_();
    }

    /**
     * Forces some of the browser proxy methods to start returning errors.
     */
    forceCertificatesError() {
      this.certificatesError_ = /** @type {!CertificatesError} */ (
          {title: 'DummyError', description: 'DummyDescription'});
    }

    /**
     * @return {!Promise} A promise that is resolved or rejected based on the
     * value of |certificatesError_|.
     * @private
     */
    fulfillRequest_() {
      return this.certificatesError_ === null ?
          Promise.resolve() :
          Promise.reject(this.certificatesError_);
    }

    /** @override */
    deleteCertificate(id) {
      this.methodCalled('deleteCertificate', id);
      return this.fulfillRequest_();
    }

    /** @override */
    exportPersonalCertificatePasswordSelected(password) {
      this.resolverMap_.get('exportPersonalCertificatePasswordSelected')
          .resolve(password);
      return this.fulfillRequest_();
    }

    /** @override */
    importPersonalCertificate(useHardwareBacked) {
      this.methodCalled('importPersonalCertificate', useHardwareBacked);
      return Promise.resolve(true);
    }

    /** @override */
    importPersonalCertificatePasswordSelected(password) {
      this.resolverMap_.get('importPersonalCertificatePasswordSelected')
          .resolve(password);
      return this.fulfillRequest_();
    }

    /** @override */
    refreshCertificates() {
      this.methodCalled('refreshCertificates');
    }

    /** @override */
    viewCertificate(id) {
      this.methodCalled('viewCertificate', id);
    }

    /** @override */
    exportCertificate(id) {
      this.methodCalled('exportCertificate', id);
    }

    /** @override */
    exportPersonalCertificate(id) {
      this.methodCalled('exportPersonalCertificate', id);
      return Promise.resolve();
    }
  }

  /** @return {!Certificate} */
  function createSampleCertificate() {
    return {
      id: 'dummyCertificateId',
      name: 'dummyCertificateName',
      subnodes: [createSampleCertificateSubnode()],
    };
  }

  /** @return {!CertificateSubnode} */
  function createSampleCertificateSubnode() {
    return {
      extractable: false,
      id: 'dummySubnodeId',
      name: 'dummySubnodeName',
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
    const kSpaceBar = 32;
    MockInteractions.keyEventOn(element, 'input', kSpaceBar);
  }

  suite('CaTrustEditDialogTests', function() {
    /** @type {?CaTrustEditDialogElement} */
    let dialog = null;

    /** @type {?TestCertificatesBrowserProxy} */
    let browserProxy = null;

    /** @type {!CaTrustInfo} */
    const caTrustInfo = {ssl: true, email: false, objSign: false};

    setup(function() {
      settings.navigateTo(settings.routes.CERTIFICATES);
      browserProxy = new TestCertificatesBrowserProxy();
      browserProxy.setCaCertificateTrust(caTrustInfo);

      certificate_manager.CertificatesBrowserProxyImpl.instance_ = browserProxy;
      PolymerTest.clearBody();
      dialog = document.createElement('ca-trust-edit-dialog');
    });

    teardown(function() {
      dialog.remove();
    });

    test('EditSuccess', function() {
      dialog.model = createSampleCertificateSubnode();
      document.body.appendChild(dialog);

      return browserProxy.whenCalled('getCaCertificateTrust')
          .then(function(id) {
            assertEquals(dialog.model.id, id);
            assertEquals(caTrustInfo.ssl, dialog.$.ssl.checked);
            assertEquals(caTrustInfo.email, dialog.$.email.checked);
            assertEquals(caTrustInfo.objSign, dialog.$.objSign.checked);

            // Simulate toggling all checkboxes.
            MockInteractions.tap(dialog.$.ssl);
            MockInteractions.tap(dialog.$.email);
            MockInteractions.tap(dialog.$.objSign);

            // Simulate clicking 'OK'.
            MockInteractions.tap(dialog.$.ok);

            return browserProxy.whenCalled('editCaCertificateTrust');
          })
          .then(function(args) {
            assertEquals(dialog.model.id, args.id);
            // Checking that the values sent to C++ are reflecting the
            // changes made by the user (toggling all checkboxes).
            assertEquals(caTrustInfo.ssl, !args.ssl);
            assertEquals(caTrustInfo.email, !args.email);
            assertEquals(caTrustInfo.objSign, !args.objSign);
            // Check that the dialog is closed.
            assertFalse(dialog.$.dialog.open);
          });
    });

    test('ImportSuccess', function() {
      dialog.model = {name: 'Dummy certificate name'};
      document.body.appendChild(dialog);

      assertFalse(dialog.$.ssl.checked);
      assertFalse(dialog.$.email.checked);
      assertFalse(dialog.$.objSign.checked);

      MockInteractions.tap(dialog.$.ssl);
      MockInteractions.tap(dialog.$.email);

      // Simulate clicking 'OK'.
      MockInteractions.tap(dialog.$.ok);
      return browserProxy.whenCalled('importCaCertificateTrustSelected')
          .then(function(args) {
            assertTrue(args.ssl);
            assertTrue(args.email);
            assertFalse(args.objSign);
          });
    });

    test('EditError', function() {
      dialog.model = createSampleCertificateSubnode();
      document.body.appendChild(dialog);
      browserProxy.forceCertificatesError();

      const whenErrorEventFired =
          test_util.eventToPromise('certificates-error', dialog);

      return browserProxy.whenCalled('getCaCertificateTrust')
          .then(function() {
            MockInteractions.tap(dialog.$.ok);
            return browserProxy.whenCalled('editCaCertificateTrust');
          })
          .then(function() {
            return whenErrorEventFired;
          });
    });
  });

  suite('CertificateDeleteConfirmationDialogTests', function() {
    /** @type {?CertificateDeleteConfirmationDialogElement} */
    let dialog = null;

    /** @type {?TestCertificatesBrowserProxy} */
    let browserProxy = null;

    /** @type {!CertificateSubnode} */
    const model = createSampleCertificateSubnode();

    setup(function() {
      settings.navigateTo(settings.routes.CERTIFICATES);
      browserProxy = new TestCertificatesBrowserProxy();
      certificate_manager.CertificatesBrowserProxyImpl.instance_ = browserProxy;
      PolymerTest.clearBody();
      dialog = document.createElement('certificate-delete-confirmation-dialog');
      dialog.model = model;
      dialog.certificateType = CertificateType.PERSONAL;
      document.body.appendChild(dialog);
    });

    teardown(function() {
      dialog.remove();
    });

    test('DeleteSuccess', function() {
      assertTrue(dialog.$.dialog.open);
      // Check that the dialog title includes the certificate name.
      const titleEl =
          Polymer.dom(dialog.$.dialog).querySelector('[slot=title]');
      assertTrue(titleEl.textContent.includes(model.name));

      // Simulate clicking 'OK'.
      MockInteractions.tap(dialog.$.ok);

      return browserProxy.whenCalled('deleteCertificate').then(function(id) {
        assertEquals(model.id, id);
        // Check that the dialog is closed.
        assertFalse(dialog.$.dialog.open);
      });
    });

    test('DeleteError', function() {
      browserProxy.forceCertificatesError();
      const whenErrorEventFired =
          test_util.eventToPromise('certificates-error', dialog);

      // Simulate clicking 'OK'.
      MockInteractions.tap(dialog.$.ok);
      return browserProxy.whenCalled('deleteCertificate').then(function(id) {
        assertEquals(model.id, id);
        // Ensure that the 'error' event was fired.
        return whenErrorEventFired;
      });
    });
  });

  suite('CertificatePasswordEncryptionDialogTests', function() {
    /** @type {?CertificatePasswordEncryptionDialogElement} */
    let dialog = null;

    /** @type {?TestCertificatesBrowserProxy} */
    let browserProxy = null;

    /** @type {!CertificateSubnode} */
    const model = createSampleCertificateSubnode();

    const methodName = 'exportPersonalCertificatePasswordSelected';

    setup(function() {
      settings.navigateTo(settings.routes.CERTIFICATES);
      browserProxy = new TestCertificatesBrowserProxy();
      certificate_manager.CertificatesBrowserProxyImpl.instance_ = browserProxy;
      PolymerTest.clearBody();
      dialog = document.createElement('certificate-password-encryption-dialog');
      dialog.model = model;
      document.body.appendChild(dialog);
    });

    teardown(function() {
      dialog.remove();
    });

    test('EncryptSuccess', function() {
      const passwordInputElements =
          Polymer.dom(dialog.$.dialog).querySelectorAll('paper-input');
      const passwordInputElement = passwordInputElements[0];
      const confirmPasswordInputElement = passwordInputElements[1];

      assertTrue(dialog.$.dialog.open);
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

      return browserProxy.whenCalled(methodName).then(function(password) {
        assertEquals(passwordInputElement.value, password);
        // Check that the dialog is closed.
        assertFalse(dialog.$.dialog.open);
      });
    });

    test('EncryptError', function() {
      browserProxy.forceCertificatesError();

      const passwordInputElements =
          Polymer.dom(dialog.$.dialog).querySelectorAll('paper-input');
      const passwordInputElement = passwordInputElements[0];
      passwordInputElement.value = 'foopassword';
      const confirmPasswordInputElement = passwordInputElements[1];
      confirmPasswordInputElement.value = passwordInputElement.value;
      triggerInputEvent(passwordInputElement);

      const whenErrorEventFired =
          test_util.eventToPromise('certificates-error', dialog);
      MockInteractions.tap(dialog.$.ok);

      return browserProxy.whenCalled(methodName).then(function() {
        return whenErrorEventFired;
      });
    });
  });

  suite('CertificatePasswordDecryptionDialogTests', function() {
    /** @type {?CertificatePasswordDecryptionDialogElement} */
    let dialog = null;

    /** @type {?TestCertificatesBrowserProxy} */
    let browserProxy = null;

    const methodName = 'importPersonalCertificatePasswordSelected';

    setup(function() {
      settings.navigateTo(settings.routes.CERTIFICATES);
      browserProxy = new TestCertificatesBrowserProxy();
      certificate_manager.CertificatesBrowserProxyImpl.instance_ = browserProxy;
      PolymerTest.clearBody();
      dialog = document.createElement('certificate-password-decryption-dialog');
      document.body.appendChild(dialog);
    });

    teardown(function() {
      dialog.remove();
    });

    test('DecryptSuccess', function() {
      const passwordInputElement =
          Polymer.dom(dialog.$.dialog).querySelector('paper-input');
      assertTrue(dialog.$.dialog.open);

      // Test that the 'OK' button is enabled even when the password field is
      // empty.
      assertEquals('', passwordInputElement.value);
      assertFalse(dialog.$.ok.disabled);

      passwordInputElement.value = 'foopassword';
      assertFalse(dialog.$.ok.disabled);

      // Simulate clicking 'OK'.
      MockInteractions.tap(dialog.$.ok);

      return browserProxy.whenCalled(methodName).then(function(password) {
        assertEquals(passwordInputElement.value, password);
        // Check that the dialog is closed.
        assertFalse(dialog.$.dialog.open);
      });
    });

    test('DecryptError', function() {
      browserProxy.forceCertificatesError();
      // Simulate entering some password.
      const passwordInputElement =
          Polymer.dom(dialog.$.dialog).querySelector('paper-input');
      passwordInputElement.value = 'foopassword';
      triggerInputEvent(passwordInputElement);

      const whenErrorEventFired =
          test_util.eventToPromise('certificates-error', dialog);
      MockInteractions.tap(dialog.$.ok);
      return browserProxy.whenCalled(methodName).then(function() {
        return whenErrorEventFired;
      });
    });
  });

  suite('CertificateSubentryTests', function() {
    let subentry = null;

    /** @type {?TestCertificatesBrowserProxy} */
    let browserProxy = null;

    /**
     * @return {!Promise} A promise firing once |CertificateActionEvent| fires.
     */
    const actionEventToPromise = function() {
      return test_util.eventToPromise(CertificateActionEvent, subentry);
    };

    setup(function() {
      settings.navigateTo(settings.routes.CERTIFICATES);
      browserProxy = new TestCertificatesBrowserProxy();
      certificate_manager.CertificatesBrowserProxyImpl.instance_ = browserProxy;
      PolymerTest.clearBody();
      subentry = document.createElement('certificate-subentry');
      subentry.model = createSampleCertificateSubnode();
      subentry.certificateType = CertificateType.PERSONAL;
      document.body.appendChild(subentry);

      // Bring up the popup menu for the following tests to use.
      MockInteractions.tap(subentry.$.dots);
      Polymer.dom.flush();
    });

    teardown(function() {
      subentry.remove();
    });

    // Test case where 'View' option is tapped.
    test('MenuOptions_View', function() {
      const viewButton = subentry.shadowRoot.querySelector('#view');
      MockInteractions.tap(viewButton);
      return browserProxy.whenCalled('viewCertificate').then(function(id) {
        assertEquals(subentry.model.id, id);
      });
    });

    // Test that the 'Edit' option is only shown when appropriate and that
    // once tapped the correct event is fired.
    test('MenuOptions_Edit', function() {
      const editButton = subentry.shadowRoot.querySelector('#edit');
      assertTrue(!!editButton);
      // Should be disabled for any certificate type other than
      // CertificateType.CA
      assertTrue(editButton.hidden);
      subentry.certificateType = CertificateType.CA;
      assertFalse(editButton.hidden);

      // Should be disabled if |policy| is true.
      const model = createSampleCertificateSubnode();
      model.policy = true;
      subentry.model = model;
      assertTrue(editButton.hidden);

      subentry.model = createSampleCertificateSubnode();
      const waitForActionEvent = actionEventToPromise();
      MockInteractions.tap(editButton);
      return waitForActionEvent.then(function(event) {
        const detail =
            /** @type {!CertificateActionEventDetail} */ (event.detail);
        assertEquals(CertificateAction.EDIT, detail.action);
        assertEquals(subentry.model.id, detail.subnode.id);
        assertEquals(subentry.certificateType, detail.certificateType);
      });
    });

    // Test that the 'Delete' option is only shown when appropriate and that
    // once tapped the correct event is fired.
    test('MenuOptions_Delete', function() {
      const deleteButton = subentry.shadowRoot.querySelector('#delete');
      assertTrue(!!deleteButton);

      // Should be disabled when 'model.readonly' is true.
      let model = createSampleCertificateSubnode();
      model.readonly = true;
      subentry.model = model;
      assertTrue(deleteButton.hidden);

      // Should be disabled when 'model.policy' is true.
      model = createSampleCertificateSubnode();
      model.policy = true;
      subentry.model = model;
      assertTrue(deleteButton.hidden);

      subentry.model = createSampleCertificateSubnode();
      const waitForActionEvent = actionEventToPromise();
      MockInteractions.tap(deleteButton);
      return waitForActionEvent.then(function(event) {
        const detail =
            /** @type {!CertificateActionEventDetail} */ (event.detail);
        assertEquals(CertificateAction.DELETE, detail.action);
        assertEquals(subentry.model.id, detail.subnode.id);
      });
    });

    // Test that the 'Export' option is always shown when the certificate type
    // is not PERSONAL and that once tapped the correct event is fired.
    test('MenuOptions_Export', function() {
      subentry.certificateType = CertificateType.SERVER;
      const exportButton = subentry.shadowRoot.querySelector('#export');
      assertTrue(!!exportButton);
      assertFalse(exportButton.hidden);
      MockInteractions.tap(exportButton);
      return browserProxy.whenCalled('exportCertificate').then(function(id) {
        assertEquals(subentry.model.id, id);
      });
    });

    // Test case of exporting a PERSONAL certificate.
    test('MenuOptions_ExportPersonal', function() {
      const exportButton = subentry.shadowRoot.querySelector('#export');
      assertTrue(!!exportButton);

      // Should be disabled when 'model.extractable' is false.
      assertTrue(exportButton.hidden);

      const model = createSampleCertificateSubnode();
      model.extractable = true;
      subentry.model = model;
      assertFalse(exportButton.hidden);

      const waitForActionEvent = actionEventToPromise();
      MockInteractions.tap(exportButton);
      return browserProxy.whenCalled('exportPersonalCertificate')
          .then(function(id) {
            assertEquals(subentry.model.id, id);

            // A promise firing once |CertificateActionEvent| is
            // fired.
            return waitForActionEvent;
          })
          .then(function(event) {
            const detail =
                /** @type {!CertificateActionEventDetail} */ (event.detail);
            assertEquals(CertificateAction.EXPORT_PERSONAL, detail.action);
            assertEquals(subentry.model.id, detail.subnode.id);
          });
    });
  });

  suite('CertificateManagerTests', function() {
    /** @type {?CertificateManagerElement} */
    let page = null;

    /** @type {?TestCertificatesBrowserProxy} */
    let browserProxy = null;

    /** @enum {number} */
    const CertificateCategoryIndex = {
      PERSONAL: 0,
      SERVER: 1,
      CA: 2,
      OTHER: 3,
    };

    setup(function() {
      settings.navigateTo(settings.routes.CERTIFICATES);
      browserProxy = new TestCertificatesBrowserProxy();
      certificate_manager.CertificatesBrowserProxyImpl.instance_ = browserProxy;
      PolymerTest.clearBody();
      page = document.createElement('certificate-manager');
      document.body.appendChild(page);
    });

    teardown(function() {
      page.remove();
    });

    /**
     * Test that the page requests information from the browser on startup and
     * that it gets populated accordingly.
     */
    test('Initialization', function() {
      // Trigger all category tabs to be added to the DOM.
      const paperTabsElement = page.shadowRoot.querySelector('paper-tabs');
      paperTabsElement.selected = CertificateCategoryIndex.PERSONAL;
      Polymer.dom.flush();
      paperTabsElement.selected = CertificateCategoryIndex.SERVER;
      Polymer.dom.flush();
      paperTabsElement.selected = CertificateCategoryIndex.CA;
      Polymer.dom.flush();
      paperTabsElement.selected = CertificateCategoryIndex.OTHER;
      Polymer.dom.flush();
      const certificateLists =
          page.shadowRoot.querySelectorAll('certificate-list');
      assertEquals(4, certificateLists.length);

      const assertCertificateListLength = function(listIndex, expectedSize) {
        const certificateEntries =
            certificateLists[listIndex].shadowRoot.querySelectorAll(
                'certificate-entry');
        assertEquals(expectedSize, certificateEntries.length);
      };

      assertCertificateListLength(CertificateCategoryIndex.PERSONAL, 0);
      assertCertificateListLength(CertificateCategoryIndex.SERVER, 0);
      assertCertificateListLength(CertificateCategoryIndex.CA, 0);
      assertCertificateListLength(CertificateCategoryIndex.OTHER, 0);

      return browserProxy.whenCalled('refreshCertificates').then(function() {
        // Simulate response for personal and CA certificates.
        cr.webUIListenerCallback(
            'certificates-changed', 'personalCerts',
            [createSampleCertificate()]);
        cr.webUIListenerCallback(
            'certificates-changed', 'caCerts',
            [createSampleCertificate(), createSampleCertificate()]);
        Polymer.dom.flush();

        assertCertificateListLength(CertificateCategoryIndex.PERSONAL, 1);
        assertCertificateListLength(CertificateCategoryIndex.SERVER, 0);
        assertCertificateListLength(CertificateCategoryIndex.CA, 2);
        assertCertificateListLength(CertificateCategoryIndex.OTHER, 0);
      });
    });

    /**
     * Tests that a dialog opens as a response to a
     * CertificateActionEvent.
     * @param {string} dialogTagName The type of dialog to test.
     * @param {CertificateActionEventDetail} eventDetail
     * @return {!Promise}
     */
    function testDialogOpensOnAction(dialogTagName, eventDetail) {
      assertFalse(!!page.shadowRoot.querySelector(dialogTagName));
      page.fire(CertificateActionEvent, eventDetail);
      Polymer.dom.flush();
      const dialog = page.shadowRoot.querySelector(dialogTagName);
      assertTrue(!!dialog);

      // Some dialogs are opened after some async operation to fetch initial
      // data. Ensure that the underlying cr-dialog is actually opened before
      // returning.
      return test_util.whenAttributeIs(dialog.$.dialog, 'open', '');
    }

    test('OpensDialog_DeleteConfirmation', function() {
      return testDialogOpensOnAction(
          'certificate-delete-confirmation-dialog',
          /** @type {!CertificateActionEventDetail} */ ({
            action: CertificateAction.DELETE,
            subnode: createSampleCertificateSubnode(),
            certificateType: CertificateType.PERSONAL
          }));
    });

    test('OpensDialog_PasswordEncryption', function() {
      return testDialogOpensOnAction(
          'certificate-password-encryption-dialog',
          /** @type {!CertificateActionEventDetail} */ ({
            action: CertificateAction.EXPORT_PERSONAL,
            subnode: createSampleCertificateSubnode(),
            certificateType: CertificateType.PERSONAL
          }));
    });

    test('OpensDialog_PasswordDecryption', function() {
      return testDialogOpensOnAction(
          'certificate-password-decryption-dialog',
          /** @type {!CertificateActionEventDetail} */ ({
            action: CertificateAction.IMPORT,
            subnode: createSampleCertificateSubnode(),
            certificateType: CertificateType.PERSONAL
          }));
    });

    test('OpensDialog_CaTrustEdit', function() {
      return testDialogOpensOnAction(
          'ca-trust-edit-dialog',
          /** @type {!CertificateActionEventDetail} */ ({
            action: CertificateAction.EDIT,
            subnode: createSampleCertificateSubnode(),
            certificateType: CertificateType.CA
          }));
    });

    test('OpensDialog_CaTrustImport', function() {
      return testDialogOpensOnAction(
          'ca-trust-edit-dialog',
          /** @type {!CertificateActionEventDetail} */ ({
            action: CertificateAction.IMPORT,
            subnode: {name: 'Dummy Certificate Name', id: null},
            certificateType: CertificateType.CA
          }));
    });
  });

  suite('CertificateListTests', function() {
    /** @type {?CertificateListElement} */
    let element = null;

    /** @type {?TestCertificatesBrowserProxy} */
    let browserProxy = null;

    setup(function() {
      settings.navigateTo(settings.routes.CERTIFICATES);
      browserProxy = new TestCertificatesBrowserProxy();
      certificate_manager.CertificatesBrowserProxyImpl.instance_ = browserProxy;
      PolymerTest.clearBody();
      element = document.createElement('certificate-list');
      document.body.appendChild(element);
    });

    teardown(function() {
      element.remove();
    });

    /**
     * Tests the "Import" button functionality.
     * @param {!CertificateType} certificateType
     * @param {string} proxyMethodName The name of the proxy method expected
     *     to be called.
     * @param {boolean} actionEventExpected Whether a
     *     CertificateActionEvent is expected to fire as a result tapping the
     *     Import button.
     * @param {boolean} bindBtn Whether to click on the import and bind btn.
     */
    function testImportForCertificateType(
        certificateType, proxyMethodName, actionEventExpected, bindBtn) {
      element.certificateType = certificateType;
      Polymer.dom.flush();

      const importButton =
          bindBtn ? element.$$('#importAndBind') : element.$$('#import');
      assertTrue(!!importButton);

      const waitForActionEvent = actionEventExpected ?
          test_util.eventToPromise(CertificateActionEvent, element) :
          Promise.resolve(null);

      MockInteractions.tap(importButton);
      return browserProxy.whenCalled(proxyMethodName)
          .then(function(arg) {
            if (proxyMethodName == 'importPersonalCertificate') {
              assertNotEquals(arg, undefined);
              assertEquals(arg, bindBtn);
            }
            return waitForActionEvent;
          })
          .then(function(event) {
            if (actionEventExpected) {
              assertEquals(CertificateAction.IMPORT, event.detail.action);
              assertEquals(certificateType, event.detail.certificateType);
            }
          });
    }

    test('ImportButton_Personal', function() {
      return testImportForCertificateType(
          CertificateType.PERSONAL, 'importPersonalCertificate', true, false);
    });

    if (cr.isChromeOS) {
      test('ImportAndBindButton_Personal', function() {
        return testImportForCertificateType(
            CertificateType.PERSONAL, 'importPersonalCertificate', true, true);
      });
    }

    test('ImportButton_Server', function() {
      return testImportForCertificateType(
          CertificateType.SERVER, 'importServerCertificate', false, false);
    });

    test('ImportButton_CA', function() {
      return testImportForCertificateType(
          CertificateType.CA, 'importCaCertificate', true, false);
    });
  });
});
