// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for interventions_internals.js
 */

/** @const {string} Path to source root. */
let ROOT_PATH = '../../../../';

/**
 * Test fixture for InterventionsInternals WebUI testing.
 * @constructor
 * @extends testing.Test
 */
function InterventionsInternalsUITest() {
  this.setupFnResolver = new PromiseResolver();
}

InterventionsInternalsUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the options page and call preLoad().
   * @override
   */
  browsePreload: 'chrome://interventions-internals',

  /** @override */
  isAsync: true,

  extraLibraries: [
    ROOT_PATH + 'third_party/mocha/mocha.js',
    ROOT_PATH + 'chrome/test/data/webui/mocha_adapter.js',
    ROOT_PATH + 'ui/webui/resources/js/promise_resolver.js',
    ROOT_PATH + 'ui/webui/resources/js/util.js',
    ROOT_PATH + 'chrome/test/data/webui/test_browser_proxy.js',
  ],

  preLoad: function() {
    /**
     * A stub class for the Mojo PageHandler.
     */
    class TestPageHandler extends TestBrowserProxy {
      constructor() {
        super(['getPreviewsEnabled']);

        /** @private {!Map} */
        this.statuses_ = new Map();
      }

      /**
       * Setup testing map.
       * @param {!Map} map The testing status map.
       */
      setTestingMap(map) {
        this.statuses_ = map;
      }

      /** @override **/
      getPreviewsEnabled() {
        this.methodCalled('getPreviewsEnabled');
        return Promise.resolve({
          statuses: this.statuses_,
        });
      }
    }

    window.setupFn = function() {
      return this.setupFnResolver.promise;
    }.bind(this);

    window.testPageHandler = new TestPageHandler();
  },
};

TEST_F('InterventionsInternalsUITest', 'DisplayCorrectStatuses', function() {
  let setupFnResolver = this.setupFnResolver;

  test('DisplayCorrectStatuses', () => {
    // Setup testPageHandler behavior.
    let testMap = new Map();
    testMap.set('params1', {
      description: 'Params 1',
      enabled: true,
    });
    testMap.set('params2', {
      description: 'Params 2',
      enabled: false,
    });
    testMap.set('params3', {
      description: 'Param 3',
      enabled: false,
    });

    window.testPageHandler.setTestingMap(testMap);
    this.setupFnResolver.resolve();

    return setupFnResolver.promise
        .then(() => {
          return window.testPageHandler.whenCalled('getPreviewsEnabled');
        })
        .then(() => {
          testMap.forEach((value, key) => {
            let expected = value.description + ': ' +
                (value.enabled ? 'Enabled' : 'Disabled');
            let actual = document.querySelector('#' + key).textContent;
            expectEquals(expected, actual);
          });
        });
  });

  mocha.run();
});

TEST_F('InterventionsInternalsUITest', 'LogNewMessage', function() {
  test('LogMessageIsPostedCorrectly', () => {
    let pageImpl = new InterventionsInternalPageImpl(null);
    let logs = [
      {
        type: 'Type_a',
        description: 'Some description_a',
        url: {url: 'Some gurl.spec()_a'},
        time: 1507221689240,  // Oct 05 2017 16:41:29 UTC
      },
      {
        type: 'Type_b',
        description: 'Some description_b',
        url: {url: 'Some gurl.spec()_b'},
        time: 758675653000,  // Jan 15 1994 23:14:13 UTC
      },
      {
        type: 'Type_c',
        description: 'Some description_c',
        url: {url: 'Some gurl.spec()_c'},
        time: -314307870000,  // Jan 16 1960 04:15:30 UTC
      },
    ];

    logs.forEach((log) => {
      pageImpl.logNewMessage(log);
    });

    let logTable = $('message-logs-table');
    let rows = logTable.querySelectorAll('.log-message');
    expectEquals(logs.length, rows.length);

    logs.forEach((log, index) => {
      let expectedTime = new Date(log.time).toISOString();
      let row = rows[index];

      expectEquals(expectedTime, row.querySelector('.log-time').textContent);
      expectEquals(log.type, row.querySelector('.log-type').textContent);
      expectEquals(
          log.description, row.querySelector('.log-description').textContent);
      expectEquals(log.url.url, row.querySelector('.log-url').textContent);
    });

  });

  mocha.run();
});
