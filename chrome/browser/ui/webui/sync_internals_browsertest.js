// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Test fixture for sync internals WebUI testing.
 * @constructor
 * @extends {testing.Test}
 */
function SyncInternalsWebUITest() {}

SyncInternalsWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the sync internals page.
   * @override
   */
  browsePreload: 'chrome://sync-internals',

  /** @override */
  preLoad: function() {
    this.makeAndRegisterMockHandler([
        'getAllNodes',
    ]);
  },

  /**
   * Checks aboutInfo's details section for the specified field.
   * @param {boolean} isValid Whether the field is valid.
   * @param {string} key The name of the key to search for in details.
   * @param {string} value The expected value if |key| is found.
   * @return {boolean} whether the field was found in the details.
   * @protected
   */
  hasInDetails: function(isValid, key, value) {
    var details = chrome.sync.aboutInfo.details;
    if (!details)
      return false;
    for (var i = 0; i < details.length; ++i) {
      if (!details[i].data)
        continue;
      for (var j = 0; j < details[i].data.length; ++j) {
        var obj = details[i].data[j];
        if (obj.stat_name == key)
          return obj.is_valid == isValid && obj.stat_value == value;
      }
    }
    return false;
  }
};

/** Constant hard-coded value to return from mock getAllNodes */
var HARD_CODED_ALL_NODES = [
{
  "BASE_SERVER_SPECIFICS": {},
      "BASE_VERSION": "1388699799780000",
      "CTIME": "Wednesday, December 31, 1969 4:00:00 PM",
      "ID": "sZ:ADqtAZw5kjSwSkukraMoMX6z0OlFXENzhA+1HZNcO6LbATQrkVenHJS5" +
          "AgICYfj8/6KcvwlCw3FIvcRFtOEP3zSP5YJ1VH53/Q==",
      "IS_DEL": false,
      "IS_DIR": true,
      "IS_UNAPPLIED_UPDATE": false,
      "IS_UNSYNCED": false,
      "LOCAL_EXTERNAL_ID": "0",
      "META_HANDLE": "376",
      "MTIME": "Wednesday, December 31, 1969 4:00:00 PM",
      "NON_UNIQUE_NAME": "Typed URLs",
      "PARENT_ID": "r",
      "SERVER_CTIME": "Wednesday, December 31, 1969 4:00:00 PM",
      "SERVER_IS_DEL": false,
      "SERVER_IS_DIR": true,
      "SERVER_MTIME": "Wednesday, December 31, 1969 4:00:00 PM",
      "SERVER_NON_UNIQUE_NAME": "Typed URLs",
      "SERVER_PARENT_ID": "r",
      "SERVER_SPECIFICS": {
        "typed_url": {
          "visit_transitions": [],
          "visits": []
        }
      },
      "SERVER_UNIQUE_POSITION": "INVALID[]",
      "SERVER_VERSION": "1388699799780000",
      "SPECIFICS": {
        "typed_url": {
          "visit_transitions": [],
          "visits": []
        }
      },
      "SYNCING": false,
      "TRANSACTION_VERSION": "1",
      "UNIQUE_BOOKMARK_TAG": "",
      "UNIQUE_CLIENT_TAG": "",
      "UNIQUE_POSITION": "INVALID[]",
      "UNIQUE_SERVER_TAG": "google_chrome_typed_urls",
      "isDirty": false,
      "serverModelType": "Typed URLs"
},
{
  "BASE_SERVER_SPECIFICS": {},
  "BASE_VERSION": "1372291923970334",
  "CTIME": "Wednesday, June 26, 2013 5:12:03 PM",
  "ID": "sZ:ADqtAZyz70DhOIusPT1v2XCd/8YT8Fy43WlqdRyH6UwoBAqMkX5Pnkl/sW9A" +
      "+AVrmzAPWFTrRBf0AWD57HyN4GcYXwSR9q4lYA==",
  "IS_DEL": false,
  "IS_DIR": false,
  "IS_UNAPPLIED_UPDATE": false,
  "IS_UNSYNCED": false,
  "LOCAL_EXTERNAL_ID": "0",
  "META_HANDLE": "3011",
  "MTIME": "Wednesday, June 26, 2013 5:12:03 PM",
  "NON_UNIQUE_NAME": "http://chrome.com/",
  "PARENT_ID": "sZ:ADqtAZw5kjSwSkukraMoMX6z0OlFXENzhA+1HZNcO6LbATQrkVen" +
      "HJS5AgICYfj8/6KcvwlCw3FIvcRFtOEP3zSP5YJ1VH53/Q==",
  "SERVER_CTIME": "Wednesday, June 26, 2013 5:12:03 PM",
  "SERVER_IS_DEL": false,
  "SERVER_IS_DIR": false,
  "SERVER_MTIME": "Wednesday, June 26, 2013 5:12:03 PM",
  "SERVER_NON_UNIQUE_NAME": "http://chrome.com/",
  "SERVER_PARENT_ID": "sZ:ADqtAZw5kjSwSkukraMoMX6z0OlFXENzhA+1HZNcO6LbAT" +
      "QrkVenHJS5AgICYfj8/6KcvwlCw3FIvcRFtOEP3zSP5YJ1VH53/Q==",
  "SERVER_SPECIFICS": {
    "typed_url": {
      "hidden": false,
      "title": "Chrome",
      "url": "http://chrome.com/",
      "visit_transitions": [
          "268435457"
          ],
      "visits": [
          "13016765523677321"
          ]
    }
  },
  "SERVER_UNIQUE_POSITION": "INVALID[]",
  "SERVER_VERSION": "1372291923970334",
  "SPECIFICS": {
    "typed_url": {
      "hidden": false,
      "title": "Chrome",
      "url": "http://chrome.com/",
      "visit_transitions": [
          "268435457"
          ],
      "visits": [
          "13016765523677321"
          ]
    }
  },
  "SYNCING": false,
  "TRANSACTION_VERSION": "1",
  "UNIQUE_BOOKMARK_TAG": "",
  "UNIQUE_CLIENT_TAG": "J28uWKpXPuQwR3SJKbuLqzYGOcM=",
  "UNIQUE_POSITION": "INVALID[]",
  "UNIQUE_SERVER_TAG": "",
  "isDirty": false,
  "serverModelType": "Typed URLs"
}
];

TEST_F('SyncInternalsWebUITest', 'Uninitialized', function() {
  assertNotEquals(null, chrome.sync.aboutInfo);
  expectTrue(this.hasInDetails(true, 'Username', ''));
  expectTrue(this.hasInDetails(false, 'Summary', 'Uninitialized'));
});

TEST_F('SyncInternalsWebUITest', 'SearchTabDoesntChangeOnItemSelect',
       function() {
  // Select the search tab.
  $('sync-search-tab').selected = true;
  expectTrue($('sync-search-tab').selected);

  // Build the data model and attach to result list.
  cr.ui.List.decorate($('sync-results-list'));
  $('sync-results-list').dataModel = new cr.ui.ArrayDataModel([
    {
      value: 'value 0',
      toString: function() { return 'node 0'; },
    },
    {
      value: 'value 1',
      toString: function() { return 'node 1'; },
    }
  ]);

  // Select the first list item and verify the search tab remains selected.
  $('sync-results-list').getListItemByIndex(0).selected = true;
  expectTrue($('sync-search-tab').selected);
});

TEST_F('SyncInternalsWebUITest', 'NodeBrowserTest', function() {
  this.mockHandler.expects(once()).getAllNodes([]).will(
      callFunction(function() {
        chrome.sync.getAllNodes.handleReply(HARD_CODED_ALL_NODES);
      }));

  // Hit the refresh button.
  $('node-browser-refresh-button').click();

  // Check that the refresh time was updated.
  expectNotEquals($('node-browser-refresh-time').textContent, 'Never');

  // Verify some hard-coded assumptions.  These depend on the vaue of the
  // hard-coded nodes, specified elsewhere in this file.

  // Start with the tree itself.
  var tree = $('sync-node-tree');
  assertEquals(1, tree.items.length);

  // Check the type root and expand it.
  var typeRoot = tree.items[0];
  expectFalse(typeRoot.expanded);
  typeRoot.expanded = true;
  assertEquals(1, typeRoot.items.length);

  // An actual sync node.  The child of the type root.
  var leaf = typeRoot.items[0];

  // Verify that selecting it affects the details view.
  expectTrue($('node-details').hasAttribute('hidden'));
  leaf.selected = true;
  expectFalse($('node-details').hasAttribute('hidden'));
});

TEST_F('SyncInternalsWebUITest', 'NodeBrowserRefreshOnTabSelect', function() {
  this.mockHandler.expects(once()).getAllNodes([]).will(
      callFunction(function() {
        chrome.sync.getAllNodes.handleReply(HARD_CODED_ALL_NODES);
      }));

  // Should start with non-refreshed node browser.
  expectEquals($('node-browser-refresh-time').textContent, 'Never');

  // Selecting the tab will refresh it.
  $('sync-browser-tab').selected = true;
  expectNotEquals($('node-browser-refresh-time').textContent, 'Never');

  // Re-selecting the tab shouldn't re-refresh.
  $('node-browser-refresh-time').textContent = 'TestCanary';
  $('sync-browser-tab').selected = false;
  $('sync-browser-tab').selected = true;
  expectEquals($('node-browser-refresh-time').textContent, 'TestCanary');
});
