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
        super(['getPreviewsEnabled', 'getPreviewsFlagsDetails']);

        /** @private {!Map} */
        this.previewsModeStatuses_ = new Map();
        this.previewsFlagsStatuses_ = new Map();
      }

      /**
       * Setup testing map for getPreviewsEnabled.
       * @param {!Map} map The testing status map.
       */
      setTestingPreviewsModeMap(map) {
        this.previewsModeStatuses_ = map;
      }

      setTestingPreviewsFlagsMap(map) {
        this.previewsFlagsStatuses_ = map;
      }

      /** @override **/
      getPreviewsEnabled() {
        this.methodCalled('getPreviewsEnabled');
        return Promise.resolve({
          statuses: this.previewsModeStatuses_,
        });
      }

      /** @override **/
      getPreviewsFlagsDetails() {
        this.methodCalled('getPreviewsFlagsDetails');
        return Promise.resolve({
          flags: this.previewsFlagsStatuses_,
        });
      }
    }

    window.setupFn = function() {
      return this.setupFnResolver.promise;
    }.bind(this);

    window.testPageHandler = new TestPageHandler();

    /**
     * Convert milliseconds to human readable date/time format.
     * The return format will be "MM/dd/YYYY hh:mm:ss.sss"
     * @param {number} time Time in millisecond since Unix Epoch.
     * @return The converted string format.
     */
    getTimeFormat = function(time) {
      let date = new Date(time);
      let options = {
        year: 'numeric',
        month: '2-digit',
        day: '2-digit',
      };

      let timeString = date.toLocaleDateString('en-US', options);
      return dateString + ' ' + date.getHours() + ':' + date.getMinutes() +
          ':' + date.getSeconds() + '.' + date.getMilliseconds();
    };

    getBlacklistedStatus = function(blacklisted) {
      return (blacklisted ? 'Blacklisted' : 'Not blacklisted');
    };
  },
};

TEST_F('InterventionsInternalsUITest', 'GetPreviewsEnabled', function() {
  let setupFnResolver = this.setupFnResolver;

  test('DisplayCorrectStatuses', () => {
    // Setup testPageHandler behavior.
    let testMap = new Map();
    testMap.set('params2', {
      description: 'Params 2',
      enabled: false,
    });
    testMap.set('params3', {
      description: 'Param 3',
      enabled: false,
    });
    testMap.set('params1', {
      description: 'Params 1',
      enabled: true,
    });

    window.testPageHandler.setTestingPreviewsModeMap(testMap);
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

          // Test correct order of statuses displayed on page.
          let statuses = document.querySelectorAll('.previews-status-value');
          for (let i = 1; i < statuses.length; i++) {
            expectGE(statuses[i].textContent, statuses[i - 1].textContent);
          }
        });
  });

  mocha.run();
});

TEST_F('InterventionsInternalsUITest', 'GetPreviewsFlagsDetails', function() {
  let setupFnResolver = this.setupFnResolver;

  test('DisplayCorrectStatuses', () => {
    // Setup testPageHandler behavior.
    let testMap = new Map();
    testMap.set('params2', {
      description: 'Params 2',
      link: 'Link 2',
      value: 'Value 2',
    });
    testMap.set('params3', {
      description: 'Param 3',
      link: 'Link 3',
      value: 'Value 3',
    });
    testMap.set('params1', {
      description: 'Params 1',
      link: 'Link 1',
      value: 'Value 1',
    });

    window.testPageHandler.setTestingPreviewsFlagsMap(testMap);
    this.setupFnResolver.resolve();

    return setupFnResolver.promise
        .then(() => {
          return window.testPageHandler.whenCalled('getPreviewsFlagsDetails');
        })
        .then(() => {
          testMap.forEach((value, key) => {
            let actualDescription =
                document.querySelector('#' + key + 'Description');
            let actualValue = document.querySelector('#' + key + 'Value');
            expectEquals(value.description, actualDescription.textContent);
            expectEquals(value.link, actualDescription.getAttribute('href'));
            expectEquals(value.value, actualValue.textContent);
          });

          // Test correct order of flags displayed on page.
          let flags = document.querySelectorAll('.previews-status-value');
          for (let i = 1; i < flags.length; i++) {
            expectGE(flags[i].textContent, flags[i - 1].textContent);
          }
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
      let expectedTime = getTimeFormat(log.time);
      let row = rows[logs.length - index - 1];  // Expecting reversed order.
                                                // (i.e. a new log message is
                                                // appended to the top of the
                                                // log table).

      expectEquals(expectedTime, row.querySelector('.log-time').textContent);
      expectEquals(log.type, row.querySelector('.log-type').textContent);
      expectEquals(
          log.description, row.querySelector('.log-description').textContent);
      expectEquals(
          log.url.url, row.querySelector('.log-url-value').textContent);
    });

  });

  mocha.run();
});

TEST_F('InterventionsInternalsUITest', 'LogNewMessageWithLongUrl', function() {
  test('LogMessageIsPostedCorrectly', () => {
    let pageImpl = new InterventionsInternalPageImpl(null);
    let log = {
      type: 'Some type',
      url: {url: ''},
      description: 'Some description',
      time: 758675653000,  // Jan 15 1994 23:14:13 UTC
    };
    // Creating long url.
    for (let i = 0; i <= 2 * URL_THRESHOLD; i++) {
      log.url.url += 'a';
    }
    let expectedUrl = log.url.url.substring(0, URL_THRESHOLD - 3) + '...';

    pageImpl.logNewMessage(log);
    expectEquals(
        expectedUrl, document.querySelector('div.log-url-value').textContent);
  });

  mocha.run();
});

TEST_F('InterventionsInternalsUITest', 'LogNewMessageWithNoUrl', function() {
  test('LogMessageIsPostedCorrectly', () => {
    let pageImpl = new InterventionsInternalPageImpl(null);
    let log = {
      type: 'Some type',
      url: {url: ''},
      description: 'Some description',
      time: 758675653000,  // Jan 15 1994 23:14:13 UTC
    };
    pageImpl.logNewMessage(log);
    let actual = $('message-logs-table').rows[1];
    let expectedNoColumns = 3;
    expectEquals(expectedNoColumns, actual.querySelectorAll('td').length);
    assert(
        !actual.querySelector('.log-url'),
        'There should not be a log-url column for empty URL');
  });

  mocha.run();
});

TEST_F('InterventionsInternalsUITest', 'AddNewBlacklistedHost', function() {
  test('AddNewBlacklistedHost', () => {
    let pageImpl = new InterventionsInternalPageImpl(null);
    let time = 758675653000;  // Jan 15 1994 23:14:13 UTC
    let expectedHost = 'example.com';
    pageImpl.onBlacklistedHost(expectedHost, time);

    let blacklistedTable = $('blacklisted-hosts-table');
    let row = blacklistedTable.querySelector('.blacklisted-host-row');

    let expectedTime = getTimeFormat(time);

    expectEquals(
        expectedHost, row.querySelector('.host-blacklisted').textContent);
    expectEquals(
        expectedTime, row.querySelector('.host-blacklisted-time').textContent);

  });

  mocha.run();
});

TEST_F('InterventionsInternalsUITest', 'HostAlreadyBlacklisted', function() {
  test('HostAlreadyBlacklisted', () => {
    let pageImpl = new InterventionsInternalPageImpl(null);
    let time0 = 758675653000;   // Jan 15 1994 23:14:13 UTC
    let time1 = 1507221689240;  // Oct 05 2017 16:41:29 UTC
    let expectedHost = 'example.com';

    pageImpl.onBlacklistedHost(expectedHost, time0);

    let blacklistedTable = $('blacklisted-hosts-table');
    let row = blacklistedTable.querySelector('.blacklisted-host-row');
    let expectedTime = getTimeFormat(time0);
    expectEquals(
        expectedHost, row.querySelector('.host-blacklisted').textContent);
    expectEquals(
        expectedTime, row.querySelector('.host-blacklisted-time').textContent);

    pageImpl.onBlacklistedHost(expectedHost, time1);

    // The row remains the same.
    expectEquals(
        expectedHost, row.querySelector('.host-blacklisted').textContent);
    expectEquals(
        expectedTime, row.querySelector('.host-blacklisted-time').textContent);
  });

  mocha.run();
});

TEST_F('InterventionsInternalsUITest', 'UpdateUserBlacklisted', function() {
  test('UpdateUserBlacklistedDisplayCorrectly', () => {
    let pageImpl = new InterventionsInternalPageImpl(null);
    let state = $('user-blacklisted-status-value');

    pageImpl.onUserBlacklistedStatusChange(true /* blacklisted */);
    expectEquals(
        getBlacklistedStatus(true /* blacklisted */), state.textContent);

    pageImpl.onUserBlacklistedStatusChange(false /* blacklisted */);
    expectEquals(
        getBlacklistedStatus(false /* blacklisted */), state.textContent);
  });

  mocha.run();
});

TEST_F('InterventionsInternalsUITest', 'OnBlacklistCleared', function() {
  test('OnBlacklistClearedRemovesAllBlacklistedHostInTable', () => {
    let pageImpl = new InterventionsInternalPageImpl(null);
    let state = $('user-blacklisted-status-value');
    let time = 758675653000;  // Jan 15 1994 23:14:13 UTC

    pageImpl.onBlacklistCleared(time);
    let actualClearedTime = $('blacklist-last-cleared-time').textContent;
    expectEquals(getTimeFormat(time), actualClearedTime);
    let blacklistedTable = $('blacklisted-hosts-table');
    let rows = blacklistedTable.querySelectorAll('.blacklisted-host-row');
    expectEquals(0, rows.length);
  });

  mocha.run();
});

TEST_F('InterventionsInternalsUITest', 'OnECTChanged', function() {
  test('UpdateETCOnChange', () => {
    let pageImpl = new InterventionsInternalPageImpl(null);
    let ectTypes = ['type1', 'type2', 'type3'];
    ectTypes.forEach((type) => {
      pageImpl.onEffectiveConnectionTypeChanged(type);
      let actual = $('nqe-type').textContent;
      expectEquals(type, actual);
    });
  });

  mocha.run();
});

TEST_F('InterventionsInternalsUITest', 'OnBlacklistIgnoreChange', function() {
  test('OnBlacklistIgnoreChangeDisable', () => {
    let pageImpl = new InterventionsInternalPageImpl(null);
    pageImpl.onIgnoreBlacklistDecisionStatusChanged(true /* ignored */);
    expectEquals('Enable Blacklist', $('ignore-blacklist-button').textContent);
    expectEquals(
        'Blacklist decisions are ignored.',
        $('blacklist-ignored-status').textContent);

    pageImpl.onIgnoreBlacklistDecisionStatusChanged(false /* ignored */);
    expectEquals('Ignore Blacklist', $('ignore-blacklist-button').textContent);
    expectEquals('', $('blacklist-ignored-status').textContent);
  });

  mocha.run();
});
