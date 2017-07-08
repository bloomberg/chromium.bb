// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs Polymer OnStartup Settings tests. */

GEN_INCLUDE(['settings_page_browsertest.js']);

/**
 * Test Polymer On Startup Settings elements.
 * @constructor
 * @extends {SettingsPageBrowserTest}
 */
function OnStartupSettingsBrowserTest() {}

OnStartupSettingsBrowserTest.prototype = {
  __proto__: SettingsPageBrowserTest.prototype,

  /** @return {Element} */
  getPageElement: function(selector) {
    var section = this.getSection(this.basicPage, 'onStartup');
    assertTrue(!!section);
    var module = section.querySelector('settings-on-startup-page');
    assertTrue(!!module);
    var result = module.$$(selector);
    assertTrue(!!result);
    return result;
  },
};

TEST_F('OnStartupSettingsBrowserTest', 'uiTests', function() {
  var self = this;

  suite('OnStartupHandler', function() {
    suiteSetup(function() {
      self.basicPage.set('pageVisibility.onStartup', true);
    });

    test('ManageStartupUrls', function() {
      /* Test that the manage startup urls button is present on the basic page.
       */
      var manageButton =
          self.getPageElement('#manage-startup-urls-subpage-trigger');
      assertTrue(!!manageButton);
    });
  });
  mocha.run();
});
