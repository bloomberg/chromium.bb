// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for the Settings layout. */

GEN_INCLUDE(['settings_page_browsertest.js']);

/**
 * @constructor
 * @extends {SettingsPageBrowserTest}
*/
function SettingsUIBrowserTest() {}

SettingsUIBrowserTest.prototype = {
  __proto__: SettingsPageBrowserTest.prototype,
};

// Times out on debug builders and may time out on memory bots because
// the Settings page can take several seconds to load in a Release build
// and several times that in a Debug build. See https://crbug.com/558434.
GEN('#if defined(MEMORY_SANITIZER) || !defined(NDEBUG)');
GEN('#define MAYBE_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All All');
GEN('#endif');

TEST_F('SettingsUIBrowserTest', 'MAYBE_All', function() {
  suite('settings-ui', function() {
    var toolbar;
    var ui;

    suiteSetup(function() {
      ui = assert(document.querySelector('settings-ui'));
    });

    test('basic', function() {
      toolbar = assert(ui.$$('cr-toolbar'));
      assertTrue(toolbar.showMenu);
    });

    test('app drawer', function(done) {
      assertEquals(null, ui.$$('settings-menu'));
      var drawer = assert(ui.$$('app-drawer'));
      assertFalse(drawer.opened);

      // Slide the drawer partway open. (These events are copied from Polymer's
      // app-drawer tests.)
      drawer.fire('track', {state: 'start'});
      drawer.fire('track', {state: 'track', dx: 10, ddx: 10});
      drawer.fire('track', {state: 'end', dx: 10, ddx: 0});

      // Menu is rendered asynchronously, but the drawer stays closed.
      Polymer.dom.flush();
      assertFalse(drawer.opened);
      assertTrue(!!ui.$$('settings-menu'));

      // Slide most of the way open.
      drawer.fire('track', {state: 'start'});
      drawer.fire('track', {state: 'track', dx: 200, ddx: 200});
      drawer.fire('track', {state: 'end', dx: 200, ddx: 0});

      // Drawer is shown.
      Polymer.dom.flush();
      assertTrue(drawer.opened);

      // Click away from the drawer.
      MockInteractions.tap(drawer.$.scrim);
      Polymer.Base.async(function() {
        // Drawer is closed, but menu is still stamped so its contents remain
        // visible as the drawer slides out.
        assertFalse(drawer.opened);
        assertTrue(!!ui.$$('settings-menu'));
        done();
      });
    });
  });

  mocha.run();
});
