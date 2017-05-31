// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Mac and Windows go to native certificate manager, and certificate manager
// isn't implemented if OpenSSL is used.
GEN('#if defined(USE_NSS_CERTS)');

GEN_INCLUDE(['options_browsertest_base.js']);

/**
 * URL of the Certificates dialog in the Settings page.
 * @const
 */
var CERTIFICATE_MANAGER_SETTINGS_PAGE_URL =
    'chrome://settings-frame/certificates';

// Standalone certificate manager dialog page is implemented only in Chrome OS.
GEN('#if defined(OS_CHROMEOS)');

/**
 * URL of the standalone certificate manager dialog page.
 * @const
 */
var CERTIFICATE_MANAGER_STANDALONE_PAGE_URL = 'chrome://certificate-manager/';

GEN('#endif  // defined(OS_CHROMEOS)');

/**
 * TestFixture for certificate manager WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function CertificateManagerWebUIBaseTest() {}

CertificateManagerWebUIBaseTest.prototype = {
  __proto__: OptionsBrowsertestBase.prototype,

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

  /** @override */
  setUp: function() {
    OptionsBrowsertestBase.prototype.setUp.call(this);

    var ariaRoleNotScopedSelectors = [
      '#tree-item-autogen-id-0',
      '#tree-item-autogen-id-1',
      '#tree-item-autogen-id-2',
      '#tree-item-autogen-id-3',
      '#tree-item-autogen-id-4',
    ];

    // Enable when failure is resolved.
    // AX_ARIA_09: http://crbug.com/570567
    this.accessibilityAuditConfig.ignoreSelectors(
        'ariaRoleNotScoped',
        ariaRoleNotScopedSelectors);

    // Enable when failure is resolved.
    // AX_ARIA_10: http://crbug.com/570566
    this.accessibilityAuditConfig.ignoreSelectors(
        'unsupportedAriaAttribute',
        '#caCertsTab-tree');

    var focusableElementNotVisibleAndNotAriaHiddenSelectors = [
      '#personalCertsTab-tree',
      '#personalCertsTab-import',
      '#personalCertsTab-import-and-bind',
      '#certificate-confirm',
    ];

    // Enable when failure is resolved.
    // AX_FOCUS_01: http://crbug.com/570568
    this.accessibilityAuditConfig.ignoreSelectors(
        'focusableElementNotVisibleAndNotAriaHidden',
        focusableElementNotVisibleAndNotAriaHiddenSelectors);
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

  /**
   * Browse to the certificate manager dialog in the Settings page.
   */
  browsePreload: CERTIFICATE_MANAGER_SETTINGS_PAGE_URL,

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
                               'policy': true },
                             { 'id': 'ca_cert2',
                               'name': 'ca_cert2',
                               'readonly': false,
                               'untrusted': false,
                               'extractable': true,
                               'policy': false }],
               }],
           ]
          ].forEach(CertificateManager.onPopulateTree);}));
  },
};

/**
 * TestFixture for testing certificate manager WebUI in the Settings page.
 * @extends {CertificateManagerWebUITest}
 * @constructor
 */
function CertificateManagerSettingsWebUITest() {}

CertificateManagerSettingsWebUITest.prototype = {
  __proto__: CertificateManagerWebUITest.prototype,

  /**
   * Browse to the certificate manager dialog in the Settings page.
   */
  browsePreload: CERTIFICATE_MANAGER_SETTINGS_PAGE_URL,
};

TEST_F('CertificateManagerSettingsWebUITest',
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

  // Click on the cancel button to verify the confirmation overlay closes.
  $('alertOverlayCancel').click();
  expectTrue($('alertOverlay').parentNode.classList.contains('transparent'));

  // Click on the delete button.
  $('personalCertsTab-delete').click();

  // Click on the ok button in the confirmation overlay.
  $('alertOverlayOk').click();
  expectTrue($('alertOverlay').parentNode.classList.contains('transparent'));

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
TEST_F('CertificateManagerSettingsWebUITest',
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

// Standalone certificate manager dialog page is implemented only in Chrome OS.
GEN('#if defined(OS_CHROMEOS)');

/**
 * TestFixture for testing standalone certificate manager WebUI.
 * @extends {CertificateManagerWebUITest}
 * @constructor
 */
function CertificateManagerStandaloneWebUITest() {}

CertificateManagerStandaloneWebUITest.prototype = {
  __proto__: CertificateManagerWebUITest.prototype,

  /**
   * Browse to the certificate manager page.
   */
  browsePreload: CERTIFICATE_MANAGER_STANDALONE_PAGE_URL,
};

// Ensure that the standalone certificate manager page loads and displays the
// ceertificates correctly.
TEST_F('CertificateManagerStandaloneWebUITest', 'testCertsDisplaying',
       function() {
  assertEquals(this.browsePreload, document.location.href);

  // Click on the first folder and get the certificates.
  var caCertsTab = $('caCertsTab');
  caCertsTab.querySelector('div.tree-item').click();
  var certs = caCertsTab.querySelectorAll('div.tree-item div.tree-item');

  // There should be exactly three certificates displayed.
  expectEquals(certs.length, 3);
});

GEN('#endif  // defined(OS_CHROMEOS)');

GEN('#endif  // defined(USE_NSS_CERTS)');
