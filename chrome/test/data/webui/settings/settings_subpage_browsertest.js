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
 *
 * @param {string} pageId 'basic' or 'advanced'.
 * @param {!Array<string>} subPages
*/
function SettingsSubPageBrowserTest(pageId, subPages) {
  /** @type {string} */
  this.pageId = pageId;

  /** @type {!Array<string>} */
  this.subPages = subPages;
}

SettingsSubPageBrowserTest.prototype = {
  __proto__: SettingsPageBrowserTest.prototype,

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
    this.hideSubPages_();
  },

  /*
   * This will hide all subpages in |this.subPages|. Note: any existing subpages
   * not listed in |this.subPages| will be shown.
   */
  hideSubPages_: function() {
    var page = this.getPage(this.pageId);
    var visibility = {};
    this.subPages.forEach(function(subPage) {
      visibility[subPage] = false;
    });
    assertEquals(0, Object.keys(page.pageVisibility).length);
    page.pageVisibility = visibility;
    // Ensure all pages are hidden.
    var sections = page.shadowRoot.querySelectorAll('settings-section');
    assertTrue(!!sections);
    assertEquals(0, sections.length);
  },

  /**
   * Ensures the subpage is initially hidden, then sets it to visible and
   * times the result, outputting a (rough) approximation of load time for the
   * subpage.
   * @param {Node} page
   * @param {string} subpage
   */
  testPage: function(page, subPage) {
    Polymer.dom.flush();
    expectFalse(!!this.getSection(page, subPage));
    var startTime = window.performance.now();
    page.set('pageVisibility.' + subPage, true);
    Polymer.dom.flush();
    var dtime = window.performance.now() - startTime;
    console.log('Page: ' + subPage + ' Load time: ' + dtime.toFixed(0) + ' ms');
    expectTrue(!!this.getSection(page, subPage));
    // Hide the page so that it doesn't interfere with other subPages.
    page.set('pageVisibility.' + subPage, false);
    Polymer.dom.flush();
  },

  testSubPages: function() {
    var page = this.getPage(this.pageId);
    this.subPages.forEach(function(subPage) {
      if (this.includePage(subPage))
        test(subPage, this.testPage.bind(this, page, subPage));
    }.bind(this));
  },

  /**
   * @param {string} id
   * @return {boolean}
   */
  includePage: function(id) {
    if (cr.isChromeOS)
      return id != 'people' && id != 'defaultBrowser';
    return id != 'internet' && id != 'users' && id != 'dateTime' &&
           id != 'bluetooth' && id != 'a11y';
  },
};

/** @constructor @extends {SettingsSubPageBrowserTest} */
function SettingsBasicSubPageBrowserTest() {
  var subPages = [
    'people',
    'internet',
    'appearance',
    'onStartup',
    'search',
    'defaultBrowser'
  ];

  SettingsSubPageBrowserTest.call(this, 'basic', subPages);
}

SettingsBasicSubPageBrowserTest.prototype = {
  __proto__: SettingsSubPageBrowserTest.prototype,
};

TEST_F('SettingsBasicSubPageBrowserTest', 'SubPages', function() {
  suite('Basic', this.testSubPages.bind(this));
  mocha.run();
});

/** @constructor @extends {SettingsSubPageBrowserTest} */
function SettingsAdvancedSubPageBrowserTest() {
  var subPages = [
    'dateTime',
    'location',
    'privacy',
    'bluetooth',
    'passwordsAndForms',
    'languages',
    'downloads',
    'reset',
    'a11y'
  ];

  SettingsSubPageBrowserTest.call(this, 'advanced', subPages);
};

SettingsAdvancedSubPageBrowserTest.prototype = {
  __proto__: SettingsSubPageBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/advanced',
};

TEST_F('SettingsAdvancedSubPageBrowserTest', 'SubPages', function() {
  suite('Advanced', this.testSubPages.bind(this));
  mocha.run();
});
