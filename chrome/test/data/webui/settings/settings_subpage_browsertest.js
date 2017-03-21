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
function SettingsSubPageBrowserTest() {
  /** @type {!Array<string>} */
  this.subPages = [];
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

  /*
   * Checks all subpages are hidden first.
   * @private
   */
  verifySubPagesHidden_: function() {
    var page = this.basicPage;
    assertEquals(0, Object.keys(page.pageVisibility).length);

    // Ensure all pages are still hidden after the dom-ifs compute their |if|.
    Polymer.dom.flush();
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
  testSubPage: function(page, subPage) {
    Polymer.dom.flush();
    assertFalse(!!this.getSection(page, subPage));
    var startTime = window.performance.now();
    page.set('pageVisibility.' + subPage, true);
    Polymer.dom.flush();
    var dtime = window.performance.now() - startTime;
    console.log('Page: ' + subPage + ' Load time: ' + dtime.toFixed(0) + ' ms');
    assertTrue(!!this.getSection(page, subPage));
    // Hide the page so that it doesn't interfere with other subPages.
    page.set('pageVisibility.' + subPage, false);
    Polymer.dom.flush();
  },

  testSubPages: function() {
    this.subPages.forEach(function(subPage) {
      test(subPage, function() {
        this.testSubPage(this.basicPage, subPage);
      }.bind(this));
    }.bind(this));
  },
};

/** @constructor @extends {SettingsSubPageBrowserTest} */
function SettingsBasicSubPageBrowserTest() {
  SettingsSubPageBrowserTest.call(this, 'basic');

  /** @override */
  this.subPages = [
    'people',
    'appearance',
    'onStartup',
    'search',
  ];
  if (cr.isChromeOS)
    this.subPages.push('internet', 'bluetooth', 'device');
  else
    this.subPages.push('defaultBrowser');
}

SettingsBasicSubPageBrowserTest.prototype = {
  __proto__: SettingsSubPageBrowserTest.prototype,
};

TEST_F('SettingsBasicSubPageBrowserTest', 'SubPages', function() {
  suiteSetup(this.verifySubPagesHidden_.bind(this));
  suite('Basic', this.testSubPages.bind(this));
  mocha.run();
});

/** @constructor @extends {SettingsSubPageBrowserTest} */
function SettingsAdvancedSubPageBrowserTest() {
  // "Advanced" sections live in the settings-basic-page.
  SettingsSubPageBrowserTest.call(this, 'basic');

  /** @override */
  this.subPages = [
    'privacy',
    'passwordsAndForms',
    'languages',
    'downloads',
    'printing',
    'a11y',
    'reset',
  ];
  if (cr.isChromeOS)
    this.subPages.push('dateTime');
  else
    this.subPages.push('system');
};

SettingsAdvancedSubPageBrowserTest.prototype = {
  __proto__: SettingsSubPageBrowserTest.prototype,

  /** @override */
  setUp: function() {
    this.toggleAdvanced();
    SettingsSubPageBrowserTest.prototype.setUp.call(this);
  },
};

TEST_F('SettingsAdvancedSubPageBrowserTest', 'SubPages', function() {
  suiteSetup(this.verifySubPagesHidden_.bind(this));
  suite('Advanced', this.testSubPages.bind(this));
  mocha.run();
});
