// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TestFixture for search engine manager WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function SearchEngineManagerWebUITest() {}

SearchEngineManagerWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the search engine manager.
   */
  browsePreload: 'chrome://settings-frame/searchEngines',
};

// Disabled due to flaky timeouts; see crbug.com/205693 .
// Test opening the search engine manager has correct location.
TEST_F('SearchEngineManagerWebUITest', 'DISABLED_testOpenSearchEngineManager',
       function() {
         assertEquals(this.browsePreload, document.location.href);
       });
