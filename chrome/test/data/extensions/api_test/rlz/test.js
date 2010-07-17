// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// RLZ api test
// browser_tests.exe --gtest_filter=ExtensionApiTest.Rlz

// If this code changes, then the corresponding code in extension_rlz_apitest.cc
// also needs to change.

chrome.test.runTests([
  function recordProductNEvent() {
    chrome.experimental.rlz.recordProductEvent('N', 'D3', 'install');
    chrome.experimental.rlz.recordProductEvent('N', 'D3', 'set-to-google');
    chrome.experimental.rlz.recordProductEvent('N', 'D3', 'first-search');

    chrome.experimental.rlz.recordProductEvent('N', 'D4', 'install');
    chrome.test.succeed();
  },

  function badProductName() {
    try {
      chrome.experimental.rlz.recordProductEvent('NN', 'D3', 'install');
      // Should not reach this line since the above call throws.
      chrome.test.fail();
    } catch(ex) {
    }

    try {
      chrome.experimental.rlz.recordProductEvent('', 'D3', 'install');
      // Should not reach this line since the above call throws.
      chrome.test.fail();
    } catch(ex) {
    }

    try {
      chrome.experimental.rlz.recordProductEvent(null, 'D3', 'install');
      // Should not reach this line since the above call throws.
      chrome.test.fail();
    } catch(ex) {
    }

    chrome.test.succeed();
  },

  function badAccessPointName() {
    try {
      chrome.experimental.rlz.recordProductEvent('N', 'D3A', 'install');
      // Should not reach this line since the above call throws.
      chrome.test.fail();
    } catch(ex) {
    }

    try {
      chrome.experimental.rlz.recordProductEvent('N', '', 'install');
      // Should not reach this line since the above call throws.
      chrome.test.fail();
    } catch(ex) {
    }

    try {
      chrome.experimental.rlz.recordProductEvent('N', null, 'install');
      // Should not reach this line since the above call throws.
      chrome.test.fail();
    } catch(ex) {
    }

    chrome.test.succeed();
  },

  function badEventName() {
    try {
      chrome.experimental.rlz.recordProductEvent('N', 'D3', 'foo');
      // Should not reach this line since the above call throws.
      chrome.test.fail();
    } catch(ex) {
    }

    try {
      chrome.experimental.rlz.recordProductEvent('N', 'D3', null);
      // Should not reach this line since the above call throws.
      chrome.test.fail();
    } catch(ex) {
    }

    chrome.test.succeed();
  },

  function recordProductDEvent() {
    chrome.experimental.rlz.recordProductEvent('D', 'D3', 'install');
    chrome.experimental.rlz.recordProductEvent('D', 'D4', 'install');
    chrome.test.succeed();
  },

  function clearProductState() {
    chrome.experimental.rlz.clearProductState('D', ['D3', 'D4']);
    chrome.test.succeed();
  },

  function getAccessPointRlz() {
    // Using an access point different then those used above so that if the
    // clearProductState() test runs before this one it does not clear the
    // rlz string tested for here.
    chrome.experimental.rlz.getAccessPointRlz('D1', function(rlzString) {
      chrome.test.assertEq('rlz_apitest', rlzString);
      chrome.test.succeed();
    });
  },

  function sendFinancialPing() {
    // Bad product.
    try {
      chrome.experimental.rlz.sendFinancialPing('', ['D3'], 'sig', 'TEST',
                                                'id', 'en', false);
      // Should not reach this line since the above call throws.
      chrome.test.fail();
    } catch(ex) {
    }

    // Bad access point list.
    try {
      chrome.experimental.rlz.sendFinancialPing('D', null, 'sig', 'TEST',
                                                'id', 'en', false);
      // Should not reach this line since the above call throws.
      chrome.test.fail();
    } catch(ex) {
    }

    // Bad access point list.
    try {
      chrome.experimental.rlz.sendFinancialPing('D', [], 'sig', 'TEST',
                                                'id', 'en', false);
      // Should not reach this line since the above call throws.
      chrome.test.fail();
    } catch(ex) {
    }

    // Valid call.
    chrome.experimental.rlz.sendFinancialPing('D', ['D3'], 'sig', 'TEST',
                                              'id', 'en', false);

    chrome.test.succeed();
  }
]);

