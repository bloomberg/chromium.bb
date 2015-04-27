// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(dbeam): test for loading upacked extensions?

GEN('#include "chrome/browser/ui/webui/extensions/' +
    'extension_settings_browsertest.h"');

// chrome/test/data/extensions/good.crx's extension ID. good.crx is loaded by
// ExtensionSettingsUIBrowserTest::InstallGoodExtension() in some of the tests.
var GOOD_CRX_ID = 'ldnnhddmnhbkjipkidpdiheffobcpfmf';

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
  isAsync: true,

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

  /** @override */
  setUp: function() {
    // Make all transitions take 0ms for testing purposes.
    var noTransitionStyle = document.createElement('style');
    noTransitionStyle.textContent =
        '* {' +
        '  -webkit-transition-duration: 0ms !important;' +
        '  -webkit-transition-delay: 0ms !important;' +
        '}';
    document.querySelector('head').appendChild(noTransitionStyle);
  },

  /**
   * Holds an array of steps that should happen in order during a test.
   * The last step should be |testDone|.
   * @protected {Array<!Function>}
   * */
  steps: [],

  /**
   * Advances to the next step in the test. Every step should call this.
   * @protected
   * */
  nextStep: function() {
    assertTrue(this.steps.length > 0);
    this.steps.shift().call(this);
  },

  /**
   * Will wait for the page to load before calling the next step. This should be
   * the first step in every test.
   * @protected
   * */
  waitForPageLoad: function() {
    assertEquals(this.browsePreload, document.location.href);
    var extensionList = getRequiredElement('extension-settings-list');
    extensionList.loadFinished.then(this.nextStep.bind(this));
  },

  /** @protected */
  verifyDeveloperModeWorks: function() {
    this.ignoreDevModeA11yFailures();
    var extensionSettings = getRequiredElement('extension-settings');
    assertFalse(extensionSettings.classList.contains('dev-mode'));
    $('toggle-dev-on').click();
    assertTrue(extensionSettings.classList.contains('dev-mode'));
    chrome.developerPrivate.getProfileConfiguration(function(profileInfo) {
      assertTrue(profileInfo.inDeveloperMode);

      // A 0ms timeout is necessary so that all the transitions can finish.
      window.setTimeout(this.nextStep.bind(this), 0);
    }.bind(this));
  },

  /** @protected */
  testDeveloperMode: function() {
    var next = this.nextStep.bind(this);
    var checkDevModeIsOff = function() {
      chrome.developerPrivate.getProfileConfiguration(function(profileInfo) {
        assertFalse(profileInfo.inDeveloperMode);
        next();
      });
    };
    this.steps = [this.waitForPageLoad,
                  checkDevModeIsOff,
                  this.verifyDeveloperModeWorks,
                  testDone];
    this.nextStep();
  },

  /**
   * TODO(hcarmona): Remove this as part of fixing crbug.com/463245.
   * Will ignore accessibility failures caused by the transition when developer
   * mode is enabled.
   * @protected
   */
  ignoreDevModeA11yFailures: function() {
    this.accessibilityAuditConfig.ignoreSelectors(
          'focusableElementNotVisibleAndNotAriaHidden',
          '#load-unpacked');
    this.accessibilityAuditConfig.ignoreSelectors(
          'focusableElementNotVisibleAndNotAriaHidden',
          '#pack-extension');
    this.accessibilityAuditConfig.ignoreSelectors(
          'focusableElementNotVisibleAndNotAriaHidden',
          '#update-extensions-now');
  },
};

// Verify that developer mode doesn't change behavior when the number of
// extensions changes.
TEST_F('ExtensionSettingsWebUITest', 'testDeveloperModeNoExtensions',
       function() {
  this.testDeveloperMode();
});

TEST_F('ExtensionSettingsWebUITest', 'testEmptyExtensionList', function() {
  var verifyListIsHiddenAndEmpty = function() {
    assertTrue($('extension-list-wrapper').hidden);
    assertFalse($('no-extensions').hidden);
    assertEquals(0, $('extension-settings-list').childNodes.length);
    this.nextStep();
  };

  this.steps = [this.waitForPageLoad, verifyListIsHiddenAndEmpty, testDone];
  this.nextStep();
});

TEST_F('ExtensionSettingsWebUITest', 'testChromeSendHandled', function() {
  var testPackExtenion = function() {
    // This dialog should be hidden at first.
    assertFalse($('pack-extension-overlay').classList.contains('showing'));

    // Show the dialog, which triggers a chrome.send() for metrics purposes.
    cr.dispatchSimpleEvent($('pack-extension'), 'click');
    assertTrue($('pack-extension-overlay').classList.contains('showing'));
    this.nextStep();
  };

  this.steps = [this.waitForPageLoad, testPackExtenion, testDone];
  this.nextStep();
});

/**
 * @param {chrome.developerPrivate.EventType} eventType
 * @param {function():void} callback
 * @constructor
 */
function UpdateListener(eventType, callback) {
  this.callback_ = callback;
  this.eventType_ = eventType;
  this.onItemStateChangedListener_ = this.onItemStateChanged_.bind(this);
  chrome.developerPrivate.onItemStateChanged.addListener(
      this.onItemStateChangedListener_);
}

UpdateListener.prototype = {
  /** @private */
  onItemStateChanged_: function(data) {
    if (this.eventType_ == data.event_type) {
      window.setTimeout(function() {
        chrome.developerPrivate.onItemStateChanged.removeListener(
            this.onItemStateChangedListener_);
        this.callback_();
      }.bind(this), 0);
    }
  }
};

function BasicExtensionSettingsWebUITest() {}

BasicExtensionSettingsWebUITest.prototype = {
  __proto__: ExtensionSettingsWebUITest.prototype,

  /** @override */
  testGenPreamble: function() {
    // Install multiple types of extensions to ensure we handle each type.
    // TODO(devlin): There are more types to add here.
    GEN('  InstallGoodExtension();');
    GEN('  InstallErrorsExtension();');
    GEN('  InstallSharedModule();');
    GEN('  InstallPackagedApp();');

    GEN('  SetAutoConfirmUninstall();');
  },

  /** @protected */
  verifyDisabledWorks: function() {
    var listener = new UpdateListener(
        chrome.developerPrivate.EventType.UNLOADED,
        function() {
      var node = getRequiredElement(GOOD_CRX_ID);
      assertTrue(node.classList.contains('inactive-extension'));
      this.nextStep();
    }.bind(this));
    chrome.management.setEnabled(GOOD_CRX_ID, false);
  },

  /** @protected */
  verifyEnabledWorks: function() {
    var listener = new UpdateListener(
        chrome.developerPrivate.EventType.LOADED,
        function() {
      var node = getRequiredElement(GOOD_CRX_ID);
      assertFalse(node.classList.contains('inactive-extension'));
      this.nextStep();
    }.bind(this));
    chrome.management.setEnabled(GOOD_CRX_ID, true);
  },

  /** @protected */
  verifyUninstallWorks: function() {
    var listener = new UpdateListener(
        chrome.developerPrivate.EventType.UNINSTALLED,
        function() {
      assertEquals(null, $(GOOD_CRX_ID));
      this.nextStep();
    }.bind(this));
    chrome.test.runWithUserGesture(function() {
      chrome.management.uninstall(GOOD_CRX_ID);
    });
  },
};

// Verify that developer mode doesn't change behavior when the number of
// extensions changes.
TEST_F('BasicExtensionSettingsWebUITest', 'testDeveloperModeManyExtensions',
       function() {
  this.testDeveloperMode();
});


TEST_F('BasicExtensionSettingsWebUITest', 'testDisable', function() {
  this.steps = [this.waitForPageLoad, this.verifyDisabledWorks, testDone];
  this.nextStep();
});

TEST_F('BasicExtensionSettingsWebUITest', 'testEnable', function() {
  this.steps = [this.waitForPageLoad,
                this.verifyDisabledWorks,
                this.verifyEnabledWorks,
                testDone];
  this.nextStep();
});

TEST_F('BasicExtensionSettingsWebUITest', 'testUninstall', function() {
  this.steps = [this.waitForPageLoad, this.verifyUninstallWorks, testDone];
  this.nextStep();
});

TEST_F('BasicExtensionSettingsWebUITest', 'testNonEmptyExtensionList',
       function() {
  var verifyListIsNotHiddenAndEmpty = function() {
    assertFalse($('extension-list-wrapper').hidden);
    assertTrue($('no-extensions').hidden);
    assertGT($('extension-settings-list').childNodes.length, 0);

    this.nextStep();
  };

  this.steps = [this.waitForPageLoad, verifyListIsNotHiddenAndEmpty, testDone];
  this.nextStep();
});

function AsyncExtensionSettingsWebUITest() {}

AsyncExtensionSettingsWebUITest.prototype = {
  __proto__: ExtensionSettingsWebUITest.prototype,

  /** @override */
  testGenPreamble: function() {
    GEN('  InstallGoodExtension();');
    GEN('  InstallErrorsExtension();');
  },
};

// Often times out on all platforms: http://crbug.com/467528
TEST_F('AsyncExtensionSettingsWebUITest',
       'DISABLED_testErrorListButtonVisibility',
    function() {
  var testButtonVisibility = function() {
    // 2 extensions are loaded:
    //   The 'good' extension will have 0 errors wich means no error list
    //     buttons.
    //   The 'bad' extension will have >3 manifest errors and <3 runtime errors.
    //     This means 2 buttons: 1 visible and 1 hidden.
    var visibleButtons = document.querySelectorAll(
        '.extension-error-list-show-more > a:not([hidden])');
    assertEquals(1, visibleButtons.length);
    // Visible buttons must be part of the focusRow.
    assertTrue(visibleButtons[0].hasAttribute('column-type'));

    var hiddenButtons = document.querySelectorAll(
        '.extension-error-list-show-more > a[hidden]');
    assertEquals(1, hiddenButtons.length);
    // Hidden buttons must NOT be part of the focusRow.
    assertFalse(hiddenButtons[0].hasAttribute('column-type'));

    this.nextStep();
  };

  this.steps = [this.waitForPageLoad,
                this.verifyDeveloperModeWorks,
                testButtonVisibility,
                testDone];
  this.nextStep();
});

/**
 * TestFixture for extension settings WebUI testing (commands config edition).
 * @extends {testing.Test}
 * @constructor
 */
function SettingsCommandsExtensionSettingsWebUITest() {}

SettingsCommandsExtensionSettingsWebUITest.prototype = {
  __proto__: ExtensionSettingsWebUITest.prototype,

  /**
   * A URL to load before starting each test.
   * @type {string}
   * @const
   */
  browsePreload: 'chrome://extensions-frame/configureCommands',
};

TEST_F('SettingsCommandsExtensionSettingsWebUITest', 'testChromeSendHandler',
    function() {
  // Just navigating to the page should trigger the chrome.send().
  var assertOverlayVisible = function() {
    assertTrue($('extension-commands-overlay').classList.contains('showing'));
    this.nextStep();
  };

  this.steps = [this.waitForPageLoad, assertOverlayVisible, testDone];
  this.nextStep();
});

/**
 * @constructor
 * @extends {ExtensionSettingsWebUITest}
 */
function InstallGoodExtensionSettingsWebUITest() {}

InstallGoodExtensionSettingsWebUITest.prototype = {
  __proto__: ExtensionSettingsWebUITest.prototype,

  /** @override */
  testGenPreamble: function() {
    GEN('  InstallGoodExtension();');
  },

  emptyTestForAccessibility() {
    this.steps = [this.waitForPageLoad, testDone];
    this.nextStep();
  },
};

TEST_F('InstallGoodExtensionSettingsWebUITest', 'testAccessibility',
       function() {
  this.emptyTestForAccessibility();
});

TEST_F('InstallGoodExtensionSettingsWebUITest', 'showOptions', function() {
  var showExtensionOptions = function() {
    var optionsOverlay = extensions.ExtensionOptionsOverlay.getInstance();
    optionsOverlay.setExtensionAndShowOverlay(GOOD_CRX_ID, 'GOOD!', '',
                                              this.nextStep.bind(this));

    // Preferred size changes don't happen in browser tests. Just fake it.
    document.querySelector('extensionoptions').onpreferredsizechanged(
        {width: 500, height: 500});
  };

  this.steps = [this.waitForPageLoad, showExtensionOptions, testDone];
  this.nextStep();
});

/**
 * @constructor
 * @extends {InstallGoodExtensionSettingsWebUITest}
 */
function ManagedExtensionSettingsWebUITest() {}

ManagedExtensionSettingsWebUITest.prototype = {
  __proto__: InstallGoodExtensionSettingsWebUITest.prototype,

  /** @override */
  testGenPreamble: function() {
    GEN('  AddManagedPolicyProvider();');
    InstallGoodExtensionSettingsWebUITest.prototype.testGenPreamble.call(this);
  },
};

TEST_F('ManagedExtensionSettingsWebUITest', 'testAccessibility', function() {
  this.emptyTestForAccessibility();
});

/**
 * @constructor
 * @extends {InstallGoodExtensionSettingsWebUITest}
 */
function OptionsDialogExtensionSettingsWebUITest() {}

OptionsDialogExtensionSettingsWebUITest.prototype = {
  __proto__: InstallGoodExtensionSettingsWebUITest.prototype,

  /** @override */
  browsePreload: ExtensionSettingsWebUITest.prototype.browsePreload +
      '?options=' + GOOD_CRX_ID,
};

TEST_F('OptionsDialogExtensionSettingsWebUITest', 'testAccessibility',
       function() {
  this.emptyTestForAccessibility();
});
