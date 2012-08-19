// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TestFixture for content settings exception area WebUI testing.
 * @extends {testing.Test}
 * @constructor
 **/
function ContentSettingsExceptionAreaWebUITest() {}

ContentSettingsExceptionAreaWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the content settings exception area.
   **/
  browsePreload: 'chrome://settings-frame/contentExceptions',
};

// Test opening the content settings exception area has correct location.
TEST_F('ContentSettingsExceptionAreaWebUITest',
       'testOpenContentSettingsExceptionArea',
       function() {
         assertEquals(this.browsePreload, document.location.href);
       });
