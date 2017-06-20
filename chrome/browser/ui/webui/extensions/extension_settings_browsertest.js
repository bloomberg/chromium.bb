// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(dbeam): test for loading upacked extensions?

GEN('#include "chrome/browser/ui/webui/extensions/' +
    'extension_settings_browsertest.h"');

// The id of the extension from |InstallGoodExtension|.
var GOOD_EXTENSION_ID = 'ldnnhddmnhbkjipkidpdiheffobcpfmf';

// The id of the extension from |InstallErrorsExtension|.
var ERROR_EXTENSION_ID = 'pdlpifnclfacjobnmbpngemkalkjamnf';

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
  browsePreload: 'chrome://extensions/',

  /** @override */
  typedefCppFixture: 'ExtensionSettingsUIBrowserTest',

  /** @override */
  setUp: function() {
    testing.Test.prototype.setUp.call(this);
    testing.Test.disableAnimationsAndTransitions();

    // Enable when failure is resolved.
    // AX_ARIA_08: http://crbug.com/560903
    this.accessibilityAuditConfig.ignoreSelectors(
      'requiredOwnedAriaRoleMissing',
      '#kiosk-app-list');
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
    // Toggling developer mode triggers a page update, so we need to be able to
    // wait for the update to finish.
    $('extension-settings-list').resetLoadFinished();
    var waitForPage = this.waitForPageLoad.bind(this);
    document.addEventListener('devControlsVisibilityUpdated',
                              function devCallback() {
      // Callback should only be handled once.
      document.removeEventListener('devControlsVisibilityUpdated', devCallback);

      chrome.developerPrivate.getProfileConfiguration(function(profileInfo) {
        assertTrue(extensionSettings.classList.contains('dev-mode'));
        assertTrue(profileInfo.inDeveloperMode);

        // This event isn't thrown because transitions are disabled.
        // Ensure transition here so that any dependent code does not break.
        ensureTransitionEndEvent($('dev-controls'), 0);

        waitForPage();
      });
    });

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

// Flaky: http://crbug.com/505506.
// Verify that developer mode doesn't change behavior when the number of
// extensions changes.
TEST_F('ExtensionSettingsWebUITest', 'DISABLED_testDeveloperModeNoExtensions',
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
      var node = getRequiredElement(GOOD_EXTENSION_ID);
      assertTrue(node.classList.contains('inactive-extension'));
      this.nextStep();
    }.bind(this));
    chrome.management.setEnabled(GOOD_EXTENSION_ID, false);
  },

  /** @protected */
  verifyEnabledWorks: function() {
    var listener = new UpdateListener(
        chrome.developerPrivate.EventType.LOADED,
        function() {
      var node = getRequiredElement(GOOD_EXTENSION_ID);
      assertFalse(node.classList.contains('inactive-extension'));
      this.nextStep();
    }.bind(this));
    chrome.management.setEnabled(GOOD_EXTENSION_ID, true);
  },

  /** @protected */
  verifyUninstallWorks: function() {
    var listener = new UpdateListener(
        chrome.developerPrivate.EventType.UNINSTALLED,
        function() {
      assertEquals(null, $(GOOD_EXTENSION_ID));
      this.nextStep();
    }.bind(this));
    chrome.test.runWithUserGesture(function() {
      chrome.management.uninstall(GOOD_EXTENSION_ID);
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

function AutoScrollExtensionSettingsWebUITest() {}

/**
 * A variation for testing auto-scroll when an id query param is passed in the
 * url.
 * @constructor
 * @extends {BasicExtensionSettingsWebUITest}
 */
AutoScrollExtensionSettingsWebUITest.prototype = {
  __proto__: BasicExtensionSettingsWebUITest.prototype,

  /** @override */
  browsePreload: 'chrome://extensions/?id=' + GOOD_EXTENSION_ID,

  /** @override */
  testGenPreamble: function() {
    BasicExtensionSettingsWebUITest.prototype.testGenPreamble.call(this);
    // The window needs to be sufficiently small in order to ensure a scroll bar
    // is available.
    GEN('  ShrinkWebContentsView();');
  },
};

TEST_F('AutoScrollExtensionSettingsWebUITest', 'testAutoScroll', function() {
  var checkHasScrollbar = function() {
    assertGT(document.scrollingElement.scrollHeight,
             document.body.clientHeight);
    this.nextStep();
  };
  var checkIsScrolled = function() {
    assertGT(document.scrollingElement.scrollTop, 0);
    this.nextStep();
  };
  var checkScrolledToTop = function() {
    assertEquals(0, document.scrollingElement.scrollTop);
    this.nextStep();
  };
  var scrollToTop = function() {
    document.scrollingElement.scrollTop = 0;
    this.nextStep();
  };
  // Test that a) autoscroll works on first page load and b) updating the
  // page doesn't result in autoscroll triggering again.
  this.steps = [this.waitForPageLoad,
                checkHasScrollbar,
                checkIsScrolled,
                scrollToTop,
                this.enableDeveloperMode,
                checkScrolledToTop,
                testDone];
  this.nextStep();
});

function ErrorConsoleExtensionSettingsWebUITest() {}

ErrorConsoleExtensionSettingsWebUITest.prototype = {
  __proto__: ExtensionSettingsWebUITest.prototype,

  /** @override */
  testGenPreamble: function() {
    GEN('  EnableErrorConsole();');
    GEN('  InstallGoodExtension();');
    GEN('  InstallErrorsExtension();');
  },
};

// Flaky on all platforms: http://crbug.com/499884, http://crbug.com/463245.
TEST_F('ErrorConsoleExtensionSettingsWebUITest',
       'DISABLED_testErrorListButtonVisibility', function() {
  var testButtonVisibility = function() {
    var extensionList = $('extension-list-wrapper');

    var visibleButtons = extensionList.querySelectorAll(
        '.errors-link:not([hidden])');
    expectEquals(1, visibleButtons.length);

    if (visibleButtons.length > 0) {
      var errorLink = $(ERROR_EXTENSION_ID).querySelector('.errors-link');
      expectEquals(visibleButtons[0], errorLink);

      var errorIcon = errorLink.querySelector('img');
      expectTrue(errorIcon.classList.contains('extension-error-warning-icon'));
    }

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
  browsePreload: 'chrome://extensions/configureCommands',
};

TEST_F('SettingsCommandsExtensionSettingsWebUITest', 'testChromeSendHandler',
    function() {
  // Just navigating to the page should trigger the chrome.send().
  var assertOverlayVisible = function() {
    assertTrue($('extension-commands-overlay').classList.contains('showing'));
    assertEquals($('extension-commands-overlay').getAttribute('aria-hidden'),
                 'false');
    this.nextStep();
  };

  this.steps = [this.waitForPageLoad, assertOverlayVisible, testDone];
  this.nextStep();
});

TEST_F('SettingsCommandsExtensionSettingsWebUITest', 'extensionSettingsUri',
    function() {
  var closeCommandOverlay = function() {
    assertTrue($('extension-commands-overlay').classList.contains('showing'));
    assertEquals($('extension-commands-overlay').getAttribute('aria-hidden'),
                 'false');
    assertEquals(window.location.href, 'chrome://extensions/configureCommands');

    // Close command overlay.
    $('extension-commands-dismiss').click();

    assertFalse($('extension-commands-overlay').classList.contains('showing'));
    assertEquals($('extension-commands-overlay').getAttribute('aria-hidden'),
                 'true');
    this.nextStep();
  };

  var checkExtensionsUrl = function() {
    // After closing the overlay, the URL shouldn't include commands overlay
    // reference.
    assertEquals(window.location.href, 'chrome://extensions/');
    this.nextStep();
  };

  this.steps = [this.waitForPageLoad,
                closeCommandOverlay,
                checkExtensionsUrl,
                testDone];
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
    optionsOverlay.setExtensionAndShow(GOOD_EXTENSION_ID, 'GOOD!', '',
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
      '?options=' + GOOD_EXTENSION_ID,
};

TEST_F('OptionsDialogExtensionSettingsWebUITest', 'testAccessibility',
       function() {
  this.emptyTestForAccessibility();
});
