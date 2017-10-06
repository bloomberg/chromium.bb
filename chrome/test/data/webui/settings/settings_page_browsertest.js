// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Prototype for Settings page tests. */

/** @const {string} Path to root from chrome/test/data/webui/settings/. */
var ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js']);

/**
 * @constructor
 * @extends {PolymerTest}
 */
function SettingsPageBrowserTest() {}

SettingsPageBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/',

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    '../fake_chrome_event.js',
    'fake_settings_private.js',
  ]),

  /** @type {?SettingsBasicPageElement} */
  basicPage: null,

  /** @override */
  setUp: function() {
    PolymerTest.prototype.setUp.call(this);
    suiteSetup(function() {
      return CrSettingsPrefs.initialized;
    });

    suiteSetup(function() {
      return this.getPage('basic').then(function(basicPage) {
        this.basicPage = basicPage;
      }.bind(this));
    }.bind(this));
  },

  /**
   * Toggles the Advanced sections.
   */
  toggleAdvanced: function() {
    var settingsMain = document.querySelector('* /deep/ settings-main');
    assert(!!settingsMain);
    settingsMain.advancedToggleExpanded = !settingsMain.advancedToggleExpanded;
    Polymer.dom.flush();
  },

  /**
   * @param {string} type The settings page type, e.g. 'about' or 'basic'.
   * @return {!PolymerElement} The PolymerElement for the page.
   */
  getPage: function(type) {
    var settingsUi = document.querySelector('settings-ui');
    assertTrue(!!settingsUi);
    var settingsMain = settingsUi.$$('settings-main');
    assertTrue(!!settingsMain);
    var pageType = 'settings-' + type + '-page';
    var page = settingsMain.$$(pageType);

    var idleRender = page && page.$$('template[is=settings-idle-load]');
    if (!idleRender)
      return Promise.resolve(page);

    return idleRender.get().then(function() {
      Polymer.dom.flush();
      return page;
    });
  },

  /**
   * @param {!PolymerElement} page The PolymerElement for the page containing
   *     |section|.
   * @param {string} section The settings page section, e.g. 'appearance'.
   * @return {Node|undefined} The DOM node for the section.
   */
  getSection: function(page, section) {
    var sections = page.shadowRoot.querySelectorAll('settings-section');
    assertTrue(!!sections);
    for (var i = 0; i < sections.length; ++i) {
      var s = sections[i];
      if (s.section == section)
        return s;
    }
    return undefined;
  },

  /**
   * Verifies the section has a visible #main element and that any possible
   * sub-pages are hidden.
   * @param {!Node} The DOM node for the section.
   */
  verifySubpagesHidden: function(section) {
    // Check if there are sub-pages to verify.
    var pages = section.querySelector('* /deep/ settings-animated-pages');
    if (!pages)
      return;

    var children = pages.getContentChildren();
    var stampedChildren = children.filter(function(element) {
      return element.tagName != 'TEMPLATE';
    });

    // The section's main child should be stamped and visible.
    var main = stampedChildren.filter(function(element) {
      return element.getAttribute('route-path') == 'default';
    });
    assertEquals(main.length, 1, 'default card not found for section ' +
        section.section);
    assertGT(main[0].offsetHeight, 0);

    // Any other stamped subpages should not be visible.
    var subpages = stampedChildren.filter(function(element) {
      return element.getAttribute('route-path') != 'default';
    });
    for (var subpage of subpages) {
      assertEquals(subpage.offsetHeight, 0, 'Expected subpage #' + subpage.id +
          ' in ' + section.section + ' not to be visible.');
    }
  },
};
