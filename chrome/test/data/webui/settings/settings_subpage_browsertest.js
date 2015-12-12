// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Suite of tests to ensure that settings subpages exist and
 * load without errors. Also outputs approximate load times for each subpage.
 */

GEN_INCLUDE(['settings_page_browsertest.js']);

/**
 * @constructor
 * @extends {SettingsPageBrowserTest}
*/
function SettingsSubPageBrowserTest() {}

SettingsSubPageBrowserTest.prototype = {
  __proto__: SettingsPageBrowserTest.prototype,

  basicPages_: [
    'people',
    'internet',
    'appearance',
    'onStartup',
    'search',
    'defaultBrowser'
  ],

  advancedPages_: [
    'dateTime',
    'location',
    'privacy',
    'bluetooth',
    'passwordsAndForms',
    'languages',
    'downloads',
    'reset',
    'a11y'
  ],

  /** @override */
  preLoad: function() {
    SettingsPageBrowserTest.prototype.preLoad.call(this);
    // This will cause all subpages to initially be hidden via dom-if.
    // This allows us to test loading each subpage independently without other
    // subpages affecting load times, etc.
    settingsHidePagesByDefaultForTest = true;
  },

  /** @override */
  setUp: function() {
    SettingsPageBrowserTest.prototype.setUp.call(this);
    // Explicitly hide all of the pages (not strictly required but is more
    // clear than relying on undefined -> hidden).
    this.hideSubPages_('basic', this.basicPages_ );
    this.hideSubPages_('advanced', this.advancedPages_ );
  },

  /*
   * This will hide all subpages in |subpages|. Note: any existing subpages
   * not listed in |subpages| will be shown.
   * @param {string} pageId
   * @param {!Array<string>} subpages
   */
  hideSubPages_: function(pageId, subpages) {
    var page = this.getPage(pageId);
    var visibility = {};
    for (var p of subpages) {
      visibility[p] = false;
    }
    assertEquals(0, Object.keys(page.pageVisibility).length);
    page.pageVisibility = visibility;
    // Ensure all pages are hidden.
    var sections = page.shadowRoot.querySelectorAll('settings-section');
    assertTrue(!!sections);
    assertEquals(0, sections.length);
  },
};

TEST_F('SettingsSubPageBrowserTest', 'SubPages', function() {
  // Assign |self| to |this| instead of binding since 'this' in suite()
  // and test() will be a Mocha 'Suite' or 'Test' instance.
  var self = this;

  // Ensures the subpage is initially hidden, then sets it to visible and
  // times the result, outputting a (rough) approximation of load time for the
  // subpage.
  var testPage = function(page, subpage) {
    Polymer.dom.flush();
    expectFalse(!!self.getSection(page, subpage), subpage);
    var startTime = window.performance.now();
    page.set('pageVisibility.' + subpage, true);
    Polymer.dom.flush();
    var dtime = window.performance.now() - startTime;
    console.log('Page: ' + subpage + ' Load time: ' + dtime.toFixed(0) + ' ms');
    expectTrue(!!self.getSection(page, subpage), subpage);
    // Hide the page so that it doesn't interfere with other subpages.
    page.set('pageVisibility.' + subpage, false);
    Polymer.dom.flush();
  };

  var includePage = function(id) {
    if (cr.isChromeOS)
      return id != 'defaultBrowser';
    return id != 'internet' && id != 'dateTime' && id != 'bluetooth' &&
           id != 'a11y';
  };

  // Register mocha tests.
  suite('Basic', function() {
    var page = self.getPage('basic');
    for (var subPage of self.basicPages_) {
      if (includePage(subPage))
        test(subPage, testPage.bind(self, page, subPage));
    }
  });

  suite('Advanced', function() {
    var page = self.getPage('advanced');
    for (var subPage of self.advancedPages_) {
      if (includePage(subPage))
        test(subPage, testPage.bind(self, page, subPage));
    }
  });

  // Run all registered tests.
  mocha.run();
});
