// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer elements. */

/** @const {string} Path to source root. */
var ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js']);

/**
 * Test fixture for shared Polymer elements.
 * @constructor
 * @extends {PolymerTest}
 */
function CrElementsBrowserTest() {}

CrElementsBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH),

  /** @override */
  get browsePreload() {
    throw 'this is abstract and should be overriden by subclasses';
  },

  /** @override */
  setUp: function() {
    PolymerTest.prototype.setUp.call(this);
    // We aren't loading the main document.
    this.accessibilityAuditConfig.ignoreSelectors('humanLangMissing', 'html');
  },
};

/**
 * @constructor
 * @extends {CrElementsBrowserTest}
 */
function CrElementsLazyRenderTest() {}

/**
 * @constructor
 * @extends {CrElementsBrowserTest}
 */
CrElementsLazyRenderTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    'cr_lazy_render_tests.js'
  ]),
};

TEST_F('CrElementsLazyRenderTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrElementsBrowserTest}
 */
function CrElementsProfileAvatarSelectorTest() {}

CrElementsProfileAvatarSelectorTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://resources/cr_elements/cr_profile_avatar_selector/' +
      'cr_profile_avatar_selector.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    'cr_profile_avatar_selector_tests.js',
  ]),
};

TEST_F('CrElementsProfileAvatarSelectorTest', 'All', function() {
  cr_profile_avatar_selector.registerTests();
  mocha.grep(cr_profile_avatar_selector.TestNames.Basic).run();
});

/**
 * @constructor
 * @extends {CrElementsBrowserTest}
 */
function CrElementsToolbarSearchFieldTest() {}

CrElementsToolbarSearchFieldTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    'cr_toolbar_search_field_tests.js',
  ]),
};

TEST_F('CrElementsToolbarSearchFieldTest', 'All', function() {
  cr_toolbar_search_field.registerTests();
  mocha.run();
});

/**
 * @constructor
 * @extends {CrElementsBrowserTest}
 */
function CrElementsSliderTest() {}

CrElementsSliderTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://resources/cr_elements/cr_slider/cr_slider.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    'cr_slider_tests.js',
  ]),
};

TEST_F('CrElementsSliderTest', 'All', function() {
  cr_slider.registerTests();
  mocha.run();
});

/**
 * @constructor
 * @extends {CrElementsBrowserTest}
 */
function CrElementsDrawerTest() {}

CrElementsDrawerTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://resources/cr_elements/cr_drawer/cr_drawer.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat(
      ['cr_drawer_tests.js', ROOT_PATH + 'ui/webui/resources/js/util.js']),
};

TEST_F('CrElementsDrawerTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrElementsBrowserTest}
 */
function CrElementsScrollableBehaviorTest() {}

CrElementsScrollableBehaviorTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://resources/cr_elements/cr_scrollable_behavior.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    'cr_scrollable_behavior_tests.js',
  ]),
};

TEST_F('CrElementsScrollableBehaviorTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrElementsBrowserTest}
 */
function CrElementsPolicyIndicatorBehaviorTest() {}

CrElementsPolicyIndicatorBehaviorTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://resources/cr_elements/policy/cr_policy_indicator_behavior.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    'cr_policy_strings.js',
    'cr_policy_indicator_behavior_tests.js',
  ]),
};

TEST_F('CrElementsPolicyIndicatorBehaviorTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrElementsBrowserTest}
 */
function CrElementsPolicyPrefIndicatorTest() {}

CrElementsPolicyPrefIndicatorTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://resources/cr_elements/policy/cr_policy_pref_indicator.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    'cr_policy_strings.js',
    'cr_policy_pref_indicator_tests.js',
  ]),
};

TEST_F('CrElementsPolicyPrefIndicatorTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrElementsBrowserTest}
 */
function CrElementsDialogTest() {}

CrElementsDialogTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://resources/cr_elements/cr_dialog/cr_dialog.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    'cr_dialog_test.js',
  ]),
};

TEST_F('CrElementsDialogTest', 'All', function() {
  mocha.run();
});
