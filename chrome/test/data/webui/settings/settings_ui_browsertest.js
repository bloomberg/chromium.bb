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
      ui.$.drawerTemplate.restamp = true;
    });

    setup(function() {
      ui.$.drawerTemplate.if = false;
      Polymer.dom.flush();
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

    test('advanced UIs stay in sync', function() {
      var main = ui.$$('settings-main');
      assertTrue(!!main);

      assertFalse(!!ui.$$('settings-menu'));
      assertFalse(ui.advancedOpened_);
      assertFalse(main.advancedToggleExpanded);

      main.advancedToggleExpanded = true;
      Polymer.dom.flush();

      assertFalse(!!ui.$$('settings-menu'));
      assertTrue(ui.advancedOpened_);
      assertTrue(main.advancedToggleExpanded);

      ui.$.drawerTemplate.if = true;
      Polymer.dom.flush();

      var menu = ui.$$('settings-menu');
      assertTrue(!!menu);
      assertTrue(menu.advancedOpened);

      MockInteractions.tap(menu.$$('#advancedPage .menu-trigger'));
      Polymer.dom.flush();

      // Check that all values are updated in unison.
      assertFalse(menu.advancedOpened);
      assertFalse(ui.advancedOpened_);
      assertFalse(main.advancedToggleExpanded);
    });

    test('URL initiated search propagates to search box', function() {
      toolbar = /** @type {!CrToolbarElement} */ (ui.$$('cr-toolbar'));
      var searchField = /** @type {CrToolbarSearchFieldElement} */ (
          toolbar.getSearchField());
      assertEquals('', searchField.getSearchInput().value);

      var query = 'foo';
      settings.navigateTo(
          settings.Route.BASIC, new URLSearchParams(`search=${query}`));
      assertEquals(query, searchField.getSearchInput().value);
    });

    test('search box initiated search propagates to URL', function() {
      var searchField = /** @type {CrToolbarSearchFieldElement} */ (
          toolbar.getSearchField());

      settings.navigateTo(
          settings.Route.BASIC, /* dynamicParams */ null,
          /* removeSearch */ true);
      assertEquals('', searchField.getSearchInput().value);
      assertFalse(settings.getQueryParameters().has('search'));

      var value = 'GOOG';
      searchField.setValue(value);
      assertEquals(value, settings.getQueryParameters().get('search'));
    });

     test('whitespace only search query is ignored', function() {
      toolbar = /** @type {!CrToolbarElement} */ (ui.$$('cr-toolbar'));
      var searchField = /** @type {CrToolbarSearchFieldElement} */ (
          toolbar.getSearchField());
      searchField.setValue('    ');
      var urlParams = settings.getQueryParameters();
      assertFalse(urlParams.has('search'));

      searchField.setValue('   foo');
      urlParams = settings.getQueryParameters();
      assertEquals('foo', urlParams.get('search'));

      searchField.setValue('   foo ');
      urlParams = settings.getQueryParameters();
      assertEquals('foo ', urlParams.get('search'));

      searchField.setValue('   ');
      urlParams = settings.getQueryParameters();
      assertFalse(urlParams.has('search'));
    });
  });

  mocha.run();
});
