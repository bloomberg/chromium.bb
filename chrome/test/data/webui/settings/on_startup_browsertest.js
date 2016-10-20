// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs Polymer OnStartup Settings tests. */

GEN_INCLUDE(['settings_page_browsertest.js']);

/**
  * Radio button enum values for restore on startup.
  * @enum
  */
var RestoreOnStartupEnum = {
  CONTINUE: 1,
  OPEN_NEW_TAB: 5,
  OPEN_SPECIFIC: 4,
};

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
    var section = this.getSection(this.getPage('basic'), 'onStartup');
    assertTrue(!!section);
    var module = section.querySelector('settings-on-startup-page');
    assertTrue(!!module);
    var result = module.$$(selector);
    assertTrue(!!result);
    return result;
  },

  /** @override */
  preLoad: function() {
    SettingsPageBrowserTest.prototype.preLoad.call(this);
    settingsHidePagesByDefaultForTest = true;
  },
};

TEST_F('OnStartupSettingsBrowserTest', 'uiTests', function() {
  /**
   * The prefs API that will get a fake implementation.
   * @type {!SettingsPrivate}
   */
  var settingsPrefs;
  var self = this;

  var restoreOnStartup = function() {
    return self.getPageElement('#onStartupRadioGroup').querySelector(
        '.iron-selected').textContent.trim();
  };

  suite('OnStartupHandler', function() {
    suiteSetup(function() {
      self.getPage('basic').set('pageVisibility.onStartup', true);
      Polymer.dom.flush();

      settingsPrefs = document.querySelector('settings-ui').$$(
          'settings-prefs');
      assertTrue(!!settingsPrefs);
      return CrSettingsPrefs.initialized;
    });

    test('open-continue', function() {
      settingsPrefs.set('prefs.session.restore_on_startup.value',
          RestoreOnStartupEnum.CONTINUE);
      assertEquals('Continue where you left off', restoreOnStartup());
    });

    test('open-ntp', function() {
      settingsPrefs.set('prefs.session.restore_on_startup.value',
          RestoreOnStartupEnum.OPEN_NEW_TAB);
      assertEquals('Open the New Tab page', restoreOnStartup());
    });

    test('open-specific', function() {
      settingsPrefs.set('prefs.session.restore_on_startup.value',
          RestoreOnStartupEnum.OPEN_SPECIFIC);
      assertEquals('Open a specific page or set of pages', restoreOnStartup());
    });
  });
  mocha.run();
});
