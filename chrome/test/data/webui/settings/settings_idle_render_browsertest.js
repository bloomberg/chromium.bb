// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for settings-idle-render. */

/** @const {string} Path to root from chrome/test/data/webui/settings/. */
var ROOT_PATH = '../../../../../';

/**
 * @constructor
 * @extends testing.Test
 */
function SettingsIdleRenderBrowserTest() {}

SettingsIdleRenderBrowserTest.prototype = {
  __proto__: testing.Test.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/controls/setting_idle_render.html',

  /** @override */
  extraLibraries: [
    ROOT_PATH + 'third_party/mocha/mocha.js',
    '../mocha_adapter.js',
  ],

  /** @override */
  isAsync: true,

  /** @override */
  runAccessibilityChecks: false,
};

TEST_F('SettingsIdleRenderBrowserTest', 'render', function() {
  // Register mocha tests.
  suite('Settings idle render tests', function() {
    setup(function() {
      var template =
          '<template is="settings-idle-render" id="idleTemplate">' +
          '  <div>' +
          '  </div>' +
          '</template>';
      document.body.innerHTML = template;
      // The div should not be initially accesible.
      assertFalse(!!document.body.querySelector('div'));
    });

    test('stamps after get()', function() {
      // Calling get() will force stamping without waiting for idle time.
      var inner = document.getElementById('idleTemplate').get();
      assertEquals('DIV', inner.nodeName);
      assertEquals(inner, document.body.querySelector('div'));
    });

    test('stamps after idle', function(done) {
      requestIdleCallback(function() {
        // After JS calls idle-callbacks, this should be accesible.
        assertTrue(!!document.body.querySelector('div'));
        done();
      });
    });
  });

  // Run all registered tests.
  mocha.run();
});
