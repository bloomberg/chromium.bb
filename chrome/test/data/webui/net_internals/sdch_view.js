// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['net_internals_test.js']);

// Anonymous namespace
(function() {

// Path to the page containing iframe. Iframe is used to load sdch-related
// content from the different origin. Otherwise favicon requests for the main
// page domain would spoil SDCH blacklists counters making test behavior hardly
// predicatble.
var BASE_PATH = 'files/sdch/base-page.html?iframe_url=';

/**
 * Checks the display on the SDCH tab against the information it should be
 * displaying.
 * @param {object} sdchInfo Results from a sdch manager info query.
 */
function checkDisplay(sdchInfo) {
  expectEquals(sdchInfo.sdch_enabled,
               $(SdchView.SDCH_ENABLED_SPAN_ID).innerText === 'true');
  expectEquals(sdchInfo.secure_scheme_support,
               $(SdchView.SECURE_SCHEME_SUPPORT_SPAN_ID).innerText === 'true');
  NetInternalsTest.checkTbodyRows(SdchView.BLACKLIST_TBODY_ID,
                                  sdchInfo.blacklisted.length);
  NetInternalsTest.checkTbodyRows(SdchView.DICTIONARIES_TBODY_ID,
                                  sdchInfo.dictionaries.length);

  // Rather than check the exact string in every position, just make sure every
  // entry does not have 'undefined' anywhere and certain entries are not empty,
  // which should find a fair number of potential output errors.
  for (var row = 0; row < sdchInfo.blacklisted.length; ++row) {
    for (var column = 0; column < 3; ++column) {
      var text = NetInternalsTest.getTbodyText(
          SdchView.BLACKLIST_TBODY_ID, row, column);
      expectNotEquals(text, '');
      expectFalse(/undefined/i.test(text));
    }
  }


  for (var row = 0; row < sdchInfo.dictionaries.length; ++row) {
    for (var column = 0; column < 6; ++column) {
      var text = NetInternalsTest.getTbodyText(
          SdchView.DICTIONARIES_TBODY_ID, row, column);
      expectFalse(/undefined/i.test(text));
      if (column === 0) {
        // At least Domain cell should not be empty.
        expectNotEquals(text, '');
      }
    }
  }
}

/**
 * A Task that loads provided page and waits for the SDCH dictionary to be
 * downloaded. The page response headers should provide Get-Dictionary header.
 * @extends {NetInternalsTest.Task}
 */
function LoadSdchDictionaryTask() {
  NetInternalsTest.Task.call(this);
}

LoadSdchDictionaryTask.prototype = {
  __proto__: NetInternalsTest.Task.prototype,

  /**
   * Navigates to the page and starts waiting to receive the results from
   * the browser process.
   */
  start: function(url) {
    g_browser.addSdchInfoObserver(this, false)
    NetInternalsTest.switchToView('sdch');
    // 127.0.0.1 is not allowed to be an SDCH domain, use test domain.
    url = url.replace('127.0.0.1', 'testdomain.com');
    this.url_ = url;
    chrome.send('loadPage', [url]);
  },

  /**
   * Callback from the BrowserBridge. Checks if |sdchInfo| has the SDCH
   * dictionary info for the dictionary the page has advertised. If so,
   * validates it and completes the task.  If not, continues running.
   * @param {object} sdchInfo Results of a SDCH manager info query.
   */
  onSdchInfoChanged: function(sdchInfo) {
    if (this.isDone())
      return;

    checkDisplay(sdchInfo);

    if (sdchInfo.dictionaries.length > 0) {
      var testDict = sdchInfo.dictionaries.filter(function(dictionary) {
        return dictionary.domain === 'sub.testdomain.com';
      });
      if (testDict.length === 0)
        return;

      expectEquals(1, testDict.length);
      var dict = testDict[0];
      expectEquals('/', dict.path);
      expectTrue(dict.url.indexOf('/files/sdch/dict') !== -1);

      var tableId = SdchView.DICTIONARIES_TBODY_ID;
      var domain = NetInternalsTest.getTbodyText(tableId, 0, 0);
      var path = NetInternalsTest.getTbodyText(tableId, 0, 1);
      var url = NetInternalsTest.getTbodyText(tableId, 0, 5);

      expectEquals(dict.domain, domain);
      expectEquals(dict.path, path);
      expectEquals(dict.url, url);

      this.onTaskDone(this.url_);
    }
  }
};

/**
 * A Task that loads provided page and waits for its domain to appear in SDCH
 * blacklist with the specified reason.
 * @param {string} reason Blacklist reason we're waiting for.
 * @extends {NetInternalsTest.Task}
 */
function LoadPageWithDecodeErrorTask(reason) {
  NetInternalsTest.Task.call(this);
  this.reason_ = reason;
}

LoadPageWithDecodeErrorTask.prototype = {
  __proto__: NetInternalsTest.Task.prototype,

  /**
   * Navigates to the page and starts waiting to receive the results from
   * the browser process.
   */
  start: function(url) {
    g_browser.addSdchInfoObserver(this, false)
    NetInternalsTest.switchToView('sdch');
    // 127.0.0.1 is not allowed to be an SDCH domain, so we need another one.
    url = url.replace('127.0.0.1', 'testdomain.com');
    chrome.send('loadPage', [url]);
  },

  /**
   * Callback from the BrowserBridge. Checks if |sdchInfo.blacklisted| contains
   * the test domain with the reason specified on creation. If so, validates it
   * and completes the task.  If not, continues running.
   * @param {object} sdchInfo Results of SDCH manager info query.
   */
  onSdchInfoChanged: function(sdchInfo) {
    if (this.isDone())
      return;

    checkDisplay(sdchInfo);

    if (sdchInfo.blacklisted.length > 0) {
      var testDomains = sdchInfo.blacklisted.filter(function(entry) {
        return entry.domain === 'sub.testdomain.com';
      });
      if (testDomains.length === 0)
        return;

      expectEquals(1, testDomains.length);
      var entry = testDomains[0];
      expectEquals(this.reason_, sdchProblemCodeToString(entry.reason));
      var tableId = SdchView.BLACKLIST_TBODY_ID;
      var domain = NetInternalsTest.getTbodyText(tableId, 0, 0);
      var reason = NetInternalsTest.getTbodyText(tableId, 0, 1);
      expectEquals(entry.domain, domain);
      expectEquals(this.reason_, reason);
      this.onTaskDone();
    }
  }
};

/**
 * Load a page, which results in downloading a SDCH dictionary. Make sure its
 * data is displayed.
 */
TEST_F('NetInternalsTest', 'netInternalsSdchViewFetchDictionary', function() {
  var taskQueue = new NetInternalsTest.TaskQueue(true);
  taskQueue.addTask(
      new NetInternalsTest.GetTestServerURLTask(
          BASE_PATH + encodeURI('/files/sdch/page.html')));
  taskQueue.addTask(new LoadSdchDictionaryTask());
  taskQueue.run();
});

/**
 * Load a page, get the dictionary for it, and get decoding error to see
 * the blacklist in action.
 */
TEST_F('NetInternalsTest', 'netInternalsSdchViewBlacklistMeta', function() {
  var taskQueue = new NetInternalsTest.TaskQueue(true);
  taskQueue.addTask(
      new NetInternalsTest.GetTestServerURLTask(
          BASE_PATH + encodeURI('/files/sdch/page.html')));
  taskQueue.addTask(new LoadSdchDictionaryTask());
  taskQueue.addTask(
      new NetInternalsTest.GetTestServerURLTask(
          BASE_PATH + encodeURI('/files/sdch/non-html')));
  taskQueue.addTask(
      new LoadPageWithDecodeErrorTask('META_REFRESH_UNSUPPORTED'));
  taskQueue.run();
});

/**
 * Load a page, which is said to be SDCH-encoded, though we don't expect it.
 */
TEST_F('NetInternalsTest', 'netInternalsSdchViewBlacklistNonSdch', function() {
  var taskQueue = new NetInternalsTest.TaskQueue(true);
  taskQueue.addTask(
      new NetInternalsTest.GetTestServerURLTask(
          BASE_PATH + encodeURI('/files/sdch/non-sdch.html')));
  taskQueue.addTask(
      new LoadPageWithDecodeErrorTask('PASSING_THROUGH_NON_SDCH'));
  taskQueue.run();
});

})();  // Anonymous namespace
