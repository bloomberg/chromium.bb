// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TestFixture for search engine manager WebUI testing.
 * @extends {testing.Test}
 * @constructor
 **/
function SearchEngineManagerWebUITest() {}

SearchEngineManagerWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the search engine manager.
   **/
  browsePreload: 'chrome://settings/searchEngines',
};

// See crosbug.com/22673
GEN('#if defined(OS_CHROMEOS)');
GEN('#define MAYBE_testOpenSearchEngineManager ' +
        'DISABLED_testOpenSearchEngineManager');
GEN('#else');
GEN('#define MAYBE_testOpenSearchEngineManager testOpenSearchEngineManager');
GEN('#endif  // defined(OS_CHROMEOS)');

// Test opening the search engine manager has correct location.
TEST_F('SearchEngineManagerWebUITest', 'MAYBE_testOpenSearchEngineManager',
       function() {
         assertEquals(this.browsePreload, document.location.href);
       });
