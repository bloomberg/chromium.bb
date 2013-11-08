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
    // TODO(zea): mock out the the sync info to fake an active syncer.
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
