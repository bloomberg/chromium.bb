// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Mac and Windows go to native certificate manager, and certificate manager
// isn't implemented if OpenSSL is used.
GEN('#if defined(USE_NSS)');

/**
 * TestFixture for certificate manager WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function CertificateManagerWebUIBaseTest() {}

CertificateManagerWebUIBaseTest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the certificate manager.
   */
  browsePreload: 'chrome://settings-frame/certificates',

  /** @override */
  preLoad: function() {
    // We can't check cr.isChromeOS in the preLoad since "cr" doesn't exist yet.
    // This is copied from ui/webui/resources/js/cr.js, maybe
    // there's a better way to do this.
    this.isChromeOS = /CrOS/.test(navigator.userAgent);

    this.makeAndRegisterMockHandler(
        [
          'editCaCertificateTrust',
          'exportPersonalCertificate',
          'importPersonalCertificate',
          'importCaCertificate',
          'exportCertificate',
          'deleteCertificate',
          'populateCertificateManager',
          'viewCertificate',
        ]);
  },
};

/**
 * TestFixture for certificate manager WebUI testing.
 * @extends {CertificateManagerWebUIBaseTest}
 * @constructor
 */
function CertificateManagerWebUIUnpopulatedTest() {}

CertificateManagerWebUIUnpopulatedTest.prototype = {
  __proto__: CertificateManagerWebUIBaseTest.prototype,

  /** @override */
  preLoad: function() {
    CertificateManagerWebUIBaseTest.prototype.preLoad.call(this);

    // We expect the populateCertificateManager callback, but do not reply to
    // it. This simulates what will be displayed if retrieving the cert list
    // from NSS is slow.
    this.mockHandler.expects(once()).populateCertificateManager();
  },
};

// Test opening the certificate manager has correct location and buttons have
// correct initial states when onPopulateTree has not been called.
TEST_F('CertificateManagerWebUIUnpopulatedTest',
       'testUnpopulatedCertificateManager', function() {
  assertEquals(this.browsePreload, document.location.href);

  // All buttons should be disabled to start.
  expectTrue($('personalCertsTab-view').disabled);
  expectTrue($('personalCertsTab-backup').disabled);
  expectTrue($('personalCertsTab-delete').disabled);
  expectTrue($('personalCertsTab-import').disabled);
  if (this.isChromeOS)
    expectTrue($('personalCertsTab-import-and-bind').disabled);

  expectTrue($('serverCertsTab-view').disabled);
  expectTrue($('serverCertsTab-export').disabled);
  expectTrue($('serverCertsTab-delete').disabled);
  expectTrue($('serverCertsTab-import').disabled);

  expectTrue($('caCertsTab-view').disabled);
  expectTrue($('caCertsTab-edit').disabled);
  expectTrue($('caCertsTab-export').disabled);
  expectTrue($('caCertsTab-delete').disabled);
  expectTrue($('caCertsTab-import').disabled);

  expectTrue($('otherCertsTab-view').disabled);
  expectTrue($('otherCertsTab-export').disabled);
  expectTrue($('otherCertsTab-delete').disabled);

  Mock4JS.verifyAllMocks();

  // If user database is not available, import buttons should be disabled.
  CertificateManager.onModelReady(false /* userDbAvailable*/,
                                  false /* tpmAvailable */);

  expectTrue($('personalCertsTab-import').disabled);
  expectTrue($('serverCertsTab-import').disabled);
  expectTrue($('caCertsTab-import').disabled);

  // Once we get the onModelReady call, the import buttons should be enabled,
  // others should still be disabled.
  CertificateManager.onModelReady(true /* userDbAvailable*/,
                                  false /* tpmAvailable */);

  expectTrue($('personalCertsTab-view').disabled);
  expectTrue($('personalCertsTab-backup').disabled);
  expectTrue($('personalCertsTab-delete').disabled);
  expectFalse($('personalCertsTab-import').disabled);

  expectTrue($('serverCertsTab-view').disabled);
  expectTrue($('serverCertsTab-export').disabled);
  expectTrue($('serverCertsTab-delete').disabled);
  expectFalse($('serverCertsTab-import').disabled);

  expectTrue($('caCertsTab-view').disabled);
  expectTrue($('caCertsTab-edit').disabled);
  expectTrue($('caCertsTab-export').disabled);
  expectTrue($('caCertsTab-delete').disabled);
  expectFalse($('caCertsTab-import').disabled);

  expectTrue($('otherCertsTab-view').disabled);
  expectTrue($('otherCertsTab-export').disabled);
  expectTrue($('otherCertsTab-delete').disabled);

  // On ChromeOS, the import and bind button should only be enabled if TPM is
  // present.
  if (this.isChromeOS) {
    expectTrue($('personalCertsTab-import-and-bind').disabled);
    CertificateManager.onModelReady(true /* userDbAvailable*/,
                                    true /* tpmAvailable */);
    expectFalse($('personalCertsTab-import-and-bind').disabled);
  }
});

/**
 * TestFixture for certificate manager WebUI testing.
 * @extends {CertificateManagerWebUIBaseTest}
 * @constructor
 */
function CertificateManagerWebUITest() {}

CertificateManagerWebUITest.prototype = {
  __proto__: CertificateManagerWebUIBaseTest.prototype,

  /** @override */
  preLoad: function() {
    CertificateManagerWebUIBaseTest.prototype.preLoad.call(this);

    var tpmAvailable = this.isChromeOS;
    var userDbAvailable = true;
    this.mockHandler.expects(once()).populateCertificateManager().will(
        callFunction(function() {
          CertificateManager.onModelReady(userDbAvailable, tpmAvailable);

          [['personalCertsTab-tree',
              [{'id': 'o1',
                'name': 'org1',
                'subnodes': [{ 'id': 'c1',
                               'name': 'cert1',
                               'readonly': false,
                               'untrusted': false,
                               'extractable': true }],
               }],
           ],
           ['caCertsTab-tree',
              [{'id': 'o2',
                'name': 'org2',
                'subnodes': [{ 'id': 'ca_cert0',
                               'name': 'ca_cert0',
                               'readonly': false,
                               'untrusted': false,
                               'extractable': true,
                               'policy': false },
                             { 'id': 'ca_cert1',
                               'name': 'ca_cert1',
                               'readonly': false,
                               'untrusted': false,
                               'extractable': true,
                               'policy': true }],
               }],
           ]
          ].forEach(CertificateManager.onPopulateTree)}));
  },
};

TEST_F('CertificateManagerWebUITest',
       'testViewAndDeleteCert', function() {
  assertEquals(this.browsePreload, document.location.href);

  this.mockHandler.expects(once()).viewCertificate(['c1']);

  expectTrue($('personalCertsTab-view').disabled);
  expectTrue($('personalCertsTab-backup').disabled);
  expectTrue($('personalCertsTab-delete').disabled);
  expectFalse($('personalCertsTab-import').disabled);
  if (this.isChromeOS)
    expectFalse($('personalCertsTab-import-and-bind').disabled);

  var personalCerts = $('personalCertsTab');

  // Click on the first folder.
  personalCerts.querySelector('div.tree-item').click();
  // Buttons should still be in same state.
  expectTrue($('personalCertsTab-view').disabled);
  expectTrue($('personalCertsTab-backup').disabled);
  expectTrue($('personalCertsTab-delete').disabled);
  expectFalse($('personalCertsTab-import').disabled);
  if (this.isChromeOS)
    expectFalse($('personalCertsTab-import-and-bind').disabled);

  // Click on the first cert.
  personalCerts.querySelector('div.tree-item div.tree-item').click();
  // Buttons should now allow you to act on the cert.
  expectFalse($('personalCertsTab-view').disabled);
  expectFalse($('personalCertsTab-backup').disabled);
  expectFalse($('personalCertsTab-delete').disabled);
  expectFalse($('personalCertsTab-import').disabled);
  if (this.isChromeOS)
    expectFalse($('personalCertsTab-import-and-bind').disabled);

  // Click on the view button.
  $('personalCertsTab-view').click();

  Mock4JS.verifyAllMocks();

  this.mockHandler.expects(once()).deleteCertificate(['c1']).will(callFunction(
      function() {
        CertificateManager.onPopulateTree(['personalCertsTab-tree', []]);
      }));

  // Click on the delete button.
  $('personalCertsTab-delete').click();
  // Click on the ok button in the confirmation overlay.
  $('alertOverlayOk').click();

  // Context sensitive buttons should be disabled.
  expectTrue($('personalCertsTab-view').disabled);
  expectTrue($('personalCertsTab-backup').disabled);
  expectTrue($('personalCertsTab-delete').disabled);
  expectFalse($('personalCertsTab-import').disabled);
  if (this.isChromeOS)
    expectFalse($('personalCertsTab-import-and-bind').disabled);
  // Tree should be empty.
  expectTrue(personalCerts.querySelector('div.tree-item') === null);
});

// Ensure certificate objects with the 'policy' property set have
// the cert-policy CSS class appended.
TEST_F('CertificateManagerWebUITest',
       'testPolicyInstalledCertificate', function() {
  // Click on the first folder and get the certificates.
  var caCertsTab = $('caCertsTab');
  caCertsTab.querySelector('div.tree-item').click();
  var certs = caCertsTab.querySelectorAll('div.tree-item div.tree-item');

  // First cert shouldn't show the controlled setting badge, and the
  // edit and delete buttons should be enabled.
  var cert0 = certs[0];
  expectEquals('ca_cert0', cert0.data.name);
  expectEquals(null, cert0.querySelector('.cert-policy'));
  cert0.click();
  expectFalse($('caCertsTab-edit').disabled);
  expectFalse($('caCertsTab-delete').disabled);

  // But the second should show the controlled setting badge, and the
  // edit and delete buttons should be disabled.
  var cert1 = certs[1];
  expectEquals('ca_cert1', cert1.data.name);
  expectNotEquals(null, cert1.querySelector('.cert-policy'));
  cert1.click();
  expectTrue($('caCertsTab-edit').disabled);
  expectTrue($('caCertsTab-delete').disabled);
});

GEN('#endif  // defined(USE_NSS)');
