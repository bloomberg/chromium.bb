// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer elements which rely on focus. */

/** @const {string} Path to source root. */
var ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_interactive_ui_test.js']);

function CrElementsFocusTest() {}

CrElementsFocusTest.prototype = {
  __proto__: PolymerInteractiveUITest.prototype,

  extraLibraries: PolymerTest.getLibraries(ROOT_PATH),
};

function CrElementsActionMenuTest() {}

CrElementsActionMenuTest.prototype = {
  __proto__: CrElementsFocusTest.prototype,

  /** @override */
  browsePreload:
      'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.html',

  extraLibraries: CrElementsFocusTest.prototype.extraLibraries.concat([
    'cr_action_menu_test.js',
  ]),
};

TEST_F('CrElementsActionMenuTest', 'All', function() {
  mocha.run();
});

function CrElementsProfileAvatarSelectorFocusTest() {}

CrElementsProfileAvatarSelectorFocusTest.prototype = {
  __proto__: CrElementsFocusTest.prototype,

  /** @override */
  browsePreload:
      'chrome://resources/cr_elements/cr_profile_avatar_selector/' +
      'cr_profile_avatar_selector.html',

  extraLibraries: CrElementsFocusTest.prototype.extraLibraries.concat([
    'cr_profile_avatar_selector_tests.js',
  ]),
};

TEST_F('CrElementsProfileAvatarSelectorFocusTest', 'All', function() {
  cr_profile_avatar_selector.registerTests();
  mocha.grep(cr_profile_avatar_selector.TestNames.Focus).run();
});
