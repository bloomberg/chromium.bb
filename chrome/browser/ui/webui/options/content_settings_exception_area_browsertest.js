// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TestFixture for content settings exception area WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function ContentSettingsExceptionAreaWebUITest() {}

ContentSettingsExceptionAreaWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /** @override */
  browsePreload: 'chrome://settings-frame/contentExceptions',
};

GEN('#if defined(OS_CHROMEOS)');
GEN('#define MAYBE_testOpenContentSettingsExceptionArea ' +
        'DISABLED_testOpenContentSettingsExceptionArea');
GEN('#else');
GEN('#define MAYBE_testOpenContentSettingsExceptionArea ' +
        'testOpenContentSettingsExceptionArea');
GEN('#endif  // defined(OS_CHROMEOS)');
// Test opening the content settings exception area has correct location.
TEST_F('ContentSettingsExceptionAreaWebUITest',
       'MAYBE_testOpenContentSettingsExceptionArea', function() {
  assertEquals(this.browsePreload, document.location.href);
});

/**
 * A class to asynchronously test the content settings exception area dialog.
 * @extends {testing.Test}
 * @constructor
 */
function ContentSettingsExceptionsAreaAsyncWebUITest() {}

ContentSettingsExceptionsAreaAsyncWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /** @override */
  browsePreload: 'chrome://settings-frame/contentExceptions',

  /** @override */
  isAsync: true,
};

// Adds and removes a location content setting exception.
TEST_F('ContentSettingsExceptionsAreaAsyncWebUITest',
       'testAddRemoveLocationExceptions', function() {
  assertEquals(this.browsePreload, document.location.href);

  /** @const */ var origin = 'http://google.com:80';
  /** @const */ var setExceptions = ContentSettings.setExceptions;

  var list = ContentSettings.getExceptionsList('cookies', 'normal');
  assertEquals(1, list.items.length);

  var setExceptionsCounter = 0;
  var setExceptionsCallback = function() {
    setExceptionsCounter++;
    if (setExceptionsCounter == 1) {
      // The first item is now the exception (edit items are always last).
      expectEquals('block', list.dataModel.item(0).setting);
      expectEquals(origin, list.dataModel.item(0).origin);

      // Delete the item and verify it worked.
      list.deleteItemAtIndex(0);
    } else if (setExceptionsCounter == 2) {
      // Verify the item was deleted, restore the original method, and finish.
      expectEquals(1, list.items.length);
      ContentSettings.setExceptions = setExceptions;
      testDone();
    }
  };

  // NOTE: if this test doesn't succeed, |ContentSettings.setExceptions| may not
  // be restored to its original method. I know no easy way to fix this.
  ContentSettings.setExceptions = function() {
    setExceptions.apply(ContentSettings, arguments);
    setExceptionsCallback();
  };

  // Add an item to the location exception area to start the test.
  list.items[0].finishEdit(origin, 'block');
});
