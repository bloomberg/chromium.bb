// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(dbeam): test for loading upacked extensions?

GEN('#include "chrome/browser/ui/webui/extensions/' +
    'extension_settings_browsertest.h"');

/**
 * Test C++ fixture for settings WebUI testing.
 * @constructor
 * @extends {testing.Test}
 */
function ExtensionSettingsUIBrowserTest() {}

/**
 * TestFixture for extension settings WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function ExtensionSettingsWebUITest() {}

ExtensionSettingsWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /** @override */
  runAccessibilityChecks: true,

  /** @override */
  accessibilityIssuesAreErrors: true,

  /**
   * A URL to load before starting each test.
   * @type {string}
   * @const
   */
  browsePreload: 'chrome://extensions-frame/',

  /** @override */
  typedefCppFixture: 'ExtensionSettingsUIBrowserTest',
};

TEST_F('ExtensionSettingsWebUITest', 'testChromeSendHandled', function() {
  assertEquals(this.browsePreload, document.location.href);

  // This dialog should be hidden at first.
  assertFalse($('pack-extension-overlay').classList.contains('showing'));

  // Show the dialog, which triggers a chrome.send() for metrics purposes.
  cr.dispatchSimpleEvent($('pack-extension'), 'click');
  assertTrue($('pack-extension-overlay').classList.contains('showing'));
});

function AsyncExtensionSettingsWebUITest() {}

AsyncExtensionSettingsWebUITest.prototype = {
  __proto__: ExtensionSettingsWebUITest.prototype,

  /** @override */
  isAsync: true,
};

TEST_F('AsyncExtensionSettingsWebUITest', 'testDeveloperModeA11y', function() {
  var devControls = $('dev-controls');

  // Make sure developer controls are hidden before checkbox is clicked.
  assertEquals(0, devControls.offsetHeight);
  $('toggle-dev-on').click();

  document.addEventListener('webkitTransitionEnd', function f(e) {
    if (e.target == devControls) {
      // Make sure developer controls are not hidden after checkbox is clicked.
      assertGT(devControls.offsetHeight, 0);

      document.removeEventListener(f, 'webkitTransitionEnd');
      testDone();
    }
  });
  ensureTransitionEndEvent(devControls, 4000);
});

/**
 * TestFixture for extension settings WebUI testing (commands config edition).
 * @extends {testing.Test}
 * @constructor
 */
function ExtensionSettingsCommandsConfigWebUITest() {}

ExtensionSettingsCommandsConfigWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /** @override */
  runAccessibilityChecks: true,

  /** @override */
  accessibilityIssuesAreErrors: true,

  /**
   * A URL to load before starting each test.
   * @type {string}
   * @const
   */
  browsePreload: 'chrome://extensions-frame/configureCommands',
};

TEST_F('ExtensionSettingsCommandsConfigWebUITest', 'testChromeSendHandler',
    function() {
  // Just navigating to the page should trigger the chrome.send().
  assertEquals(this.browsePreload, document.location.href);
  assertTrue($('extension-commands-overlay').classList.contains('showing'));
});

/**
 * @constructor
 * @extends {ExtensionSettingsWebUITest}
 */
function ExtensionSettingsWebUITestWithExtensionInstalled() {}

ExtensionSettingsWebUITestWithExtensionInstalled.prototype = {
  __proto__: ExtensionSettingsWebUITest.prototype,

  /** @override */
  typedefCppFixture: 'ExtensionSettingsUIBrowserTest',

  /** @override */
  testGenPreamble: function() {
    GEN('  InstallGoodExtension();');
  }
};

/** @this {ExtensionSettingsWebUITestWithExtensionInstalled} */
function runAudit() {
  assertEquals(this.browsePreload, document.location.href);
  this.runAccessibilityAudit();
}

TEST_F('ExtensionSettingsWebUITestWithExtensionInstalled',
       'baseAccessibilityIsOk', runAudit);

/**
 * @constructor
 * @extends {ExtensionSettingsWebUITestWithExtensionInstalled}
 */
function ManagedExtensionSettingsWebUITest() {}

ManagedExtensionSettingsWebUITest.prototype = {
  __proto__: ExtensionSettingsWebUITestWithExtensionInstalled.prototype,

  /** @override */
  testGenPreamble: function() {
    GEN('  AddManagedPolicyProvider();');
    ExtensionSettingsWebUITestWithExtensionInstalled.prototype.testGenPreamble.
        call(this);
  },
};

TEST_F('ManagedExtensionSettingsWebUITest', 'testAccessibility', runAudit);

/**
 * @constructor
 * @extends {ExtensionSettingsWebUITestWithExtensionInstalled}
 */
function ExtensionOptionsDialogsWebUITest() {}

ExtensionOptionsDialogsWebUITest.prototype = {
  __proto__: ExtensionSettingsWebUITestWithExtensionInstalled.prototype,

  /** @override */
  browsePreload: ExtensionSettingsWebUITest.prototype.browsePreload +
      '?options=ldnnhddmnhbkjipkidpdiheffobcpfmf',
};

TEST_F('ExtensionOptionsDialogsWebUITest', 'testAccessibility', runAudit);
