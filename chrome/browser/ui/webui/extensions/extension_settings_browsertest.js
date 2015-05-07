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
    testing.Test.disableAnimationsAndTransitions();
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
  enableDeveloperMode: function() {
    var next = this.nextStep.bind(this);
    extensions.ExtensionSettings.getInstance().testingDeveloperModeCallback =
        function() {
      chrome.developerPrivate.getProfileConfiguration(function(profileInfo) {
        assertTrue(extensionSettings.classList.contains('dev-mode'));
        assertTrue(profileInfo.inDeveloperMode);
        next();

        // This event isn't thrown because transitions are disabled.
        // Ensure transition here so that any dependent code does not break.
        ensureTransitionEndEvent($('dev-controls'), 0);
      });
    };

    var extensionSettings = getRequiredElement('extension-settings');
    assertFalse(extensionSettings.classList.contains('dev-mode'));
    $('toggle-dev-on').click();
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
                  this.enableDeveloperMode,
                  testDone];
    this.nextStep();
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

// Still fails on CrWinClang tester. BUG=463245
TEST_F('AsyncExtensionSettingsWebUITest',
       'DISABLED_testErrorListButtonVisibility',
    function() {
  var testButtonVisibility = function() {
    var extensionList = $('extension-list-wrapper');

    // 2 extensions are loaded:
    //   The 'good' extension will have 0 errors wich means no error button.
    //   The 'bad' extension will have >3 manifest errors and <3 runtime errors.
    //     This means there will be a single error button.
    var visibleButtons = extensionList.querySelectorAll(
        '.errors-link:not([hidden])');
    expectEquals(1, visibleButtons.length);

    var hiddenButtons = extensionList.querySelectorAll('.errors-link[hidden]');
    expectEquals(1, hiddenButtons.length);

    this.nextStep();
  };

  this.steps = [this.waitForPageLoad,
                this.enableDeveloperMode,
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
