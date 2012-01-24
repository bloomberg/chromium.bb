// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include the C++ test fixture.
GEN('#include "chrome/browser/ui/webui/' +
    'ssl_client_certificate_selector_webui_browsertest.h"');

/**
 * Mocked site for the SSL client certificate viewer. Used to validate
 * initialization of the dialog.
 * @type {string}
 */
var site = 'www.mysite.com:123';

/**
 * Mock certificate name used for validating initializaiton of the dialog.
 * @type {string}
 */
var certName1 = 'Foo certificate';

/**
 * Mock certificate name used for validating initializaiton of the dialog.
 * @type {string}
 */
var certName2 = 'Bar certificate';

/**
 * Mock certificate details used for validating initialziation of the dialog.
 * @type {string}
 */
var certDetails1 = 'Foo details';

/**
 * Mock certificate details used to verify proper update of details when the
 * selected certificate changes.
 * @type {string}
 */
var certDetails2 = 'Bar details';

/**
 * Index of the certificate to select for validating the certificate update
 * process.
 * @type {number}
 */
var certSelectIndex = 1;

/**
 * Number of mock certificates.
 * @type {number}
 */
var certCount = 2;

/**
 * Test fixture for SSL client certificate selector tests.
 * @extends {testing.Test}
 */
function SSLClientCertificateSelectorUITest() {}

SSLClientCertificateSelectorUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Expected result for a triggered action.  The result is set within mocked
   * function calls.
   * @type {number|null|undefined}
   * @private
   */
  result_: undefined,

  /**
   * Define the C++ fixture class.
   * @type {?string}
   * @override
   */
  typedefCppFixture: 'SSLClientCertificateSelectorUITest',

  /**
   * Show the client certificate selector.
   */
  testGenPreamble: function() {
    GEN('ShowSSLClientCertificateSelector();');
  },

  /**
   * Mock interaction with C++.
   */
  preLoad: function() {
    this.makeAndRegisterMockHandler(['DialogClose',
                                     'requestDetails',
                                     'viewCertificate']);

    // The argument for 'DialogClose' contains the index of the selected
    // certificate or null if cancelling the dialog.  Mock the call to prevent
    // closing of the dialog, which in turn allows proper shutdown of the test.
    // Store the selection for validation.
    var self = this;
    var savedArgs = new SaveMockArguments();
    this.mockHandler.stubs().DialogClose(savedArgs.match(NOT_NULL)).
        will(callFunctionWithSavedArgs(savedArgs, function(args) {
          self.result_ = JSON.parse(args)[0];
        }));

    // Mock retrieval of the certificate information for populating the dialog.
    // Details are fetched once during initialization of the dialog.
    this.mockHandler.expects(once()).requestDetails().
        will(callFunction(function() {
          sslClientCertificateSelector.setDetails(
              {'site': site,
               'certificates': [certName1, certName2],
               'details': [certDetails1, certDetails2]});
        }));

    // The argument for 'viewCertificate' is the index of the selected
    // certificate.  Store the index for validation.
    this.mockHandler.stubs().viewCertificate(savedArgs.match(NOT_NULL)).
      will(callFunctionWithSavedArgs(savedArgs, function(args) {
        self.result_ = args[0];
      }));
  }
};

/**
 * Verifies that fields in the dialog are properly initialized.
 */
TEST_F('SSLClientCertificateSelectorUITest', 'initializationTest', function() {
  assertFalse($('info').disabled);
  assertFalse($('ok').disabled);
  assertFalse($('cancel').disabled);
  assertEquals(site, $('site').textContent);
  var children = $('certificates').childNodes;
  assertEquals(certCount, children.length, 'Number of certificates');
  assertEquals(0, sslClientCertificateSelector.getSelectedCertificateIndex(),
      'Default certificate index');
  assertEquals(certName1, children[0].textContent);
  assertEquals(certName2, children[1].textContent);
  assertEquals(certDetails1, $('details').textContent);
});

/**
 * Verifies that cancel results in a null selection.
 */
TEST_F('SSLClientCertificateSelectorUITest', 'cancelTest', function() {
  assertEquals(undefined, this.result_);
  $('cancel').click();
  assertEquals(null, this.result_);
  assertTrue($('info').disabled);
  assertTrue($('ok').disabled);
  assertTrue($('cancel').disabled);
});

/**
 * Verifies that controls are disabled, the proper cert details are displayed
 * and that the correct certificate index is selected.
 */
TEST_F('SSLClientCertificateSelectorUITest', 'selectionTest', function() {
  assertEquals(undefined, this.result_);
  sslClientCertificateSelector.setSelectedCertificateIndex(certSelectIndex);
  assertEquals(certDetails2, $('details').textContent);
  $('ok').click();
  assertEquals(certSelectIndex, this.result_);
  assertTrue($('info').disabled);
  assertTrue($('ok').disabled);
  assertTrue($('cancel').disabled);
});

/**
 * Verifies that the correct certificate index is selected when requesting
 * certificate details.
 */
TEST_F('SSLClientCertificateSelectorUITest', 'showDetailsTest', function() {
  assertEquals(undefined, this.result_);
  $('info').click();
  assertEquals(0, this.result_);
  sslClientCertificateSelector.setSelectedCertificateIndex(certSelectIndex);
  $('info').click();
  assertEquals(certSelectIndex, this.result_);
});

