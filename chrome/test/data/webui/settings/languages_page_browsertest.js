// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for settings-languages-page. */

/** @const {string} Path to root from chrome/test/data/webui/settings/. */
var ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js']);
// SettingsPageBrowserTest fixture.
GEN_INCLUDE([ROOT_PATH +
             'chrome/test/data/webui/settings/settings_page_browsertest.js']);

/**
 * Test class for settings-languages-page UI.
 * @constructor
 * @extends {SettingsPageBrowserTest}
 */
function SettingsLanguagesPageBrowserTest() {}

SettingsLanguagesPageBrowserTest.prototype = {
  __proto__: SettingsPageBrowserTest.prototype,

  /** @override */
  preLoad: function() {
    SettingsPageBrowserTest.prototype.preLoad.call(this);
    settingsHidePagesByDefaultForTest = true;
  },
};

// Flaky on Windows. See https://crbug.com/641400.
// May time out on debug builders and memory bots because
// the Settings page can take several seconds to load in a Release build
// and several times that in a Debug build. See https://crbug.com/558434.
GEN('#if defined(OS_WINDOWS) || defined(MEMORY_SANITIZER) || !defined(NDEBUG)');
GEN('#define MAYBE_LanguagesPage DISABLED_LanguagesPage');
GEN('#else');
GEN('#define MAYBE_LanguagesPage LanguagesPage');
GEN('#endif');

// Runs languages page tests.
TEST_F('SettingsLanguagesPageBrowserTest', 'MAYBE_LanguagesPage', function() {
  suite('languages page', function() {
    testing.Test.disableAnimationsAndTransitions();

    this.toggleAdvanced();
    var advanced = this.getPage('advanced');

    var languagesSection;
    var languagesPage;
    var languagesCollapse;
    var languageHelper;

    /**
     * @param {numExpected} Expected number of languages to eventually be
     *     enabled.
     * @return {!Promise} Resolved when the number of enabled languages changes
     *     to match expectations.
     */
    function whenNumEnabledLanguagesBecomes(numExpected) {
      assert(!!languagesPage);
      return new Promise(function(resolve, reject) {
        languagesPage.addEventListener('languages-changed', function changed() {
          if (languagesPage.languages.enabled.length != numExpected)
            return;
          resolve();
          languagesPage.removeEventListener('languages-changed', changed);
        });
      });
    }

    // Returns a supported language that is not enabled, for testing.
    function getAvailableLanguage() {
      return languagesPage.languages.supported.find(function(language) {
        return !languageHelper.isLanguageEnabled(language.code);
      });
    }

    suiteSetup(function() {
      advanced.set('pageVisibility.languages', true);
      Polymer.dom.flush();

      languagesSection = assert(this.getSection(advanced, 'languages'));
      languagesPage = assert(
          languagesSection.querySelector('settings-languages-page'));
      languagesCollapse = languagesPage.$.languagesCollapse;
      languagesCollapse.opened = true;

      languageHelper = languagesPage.languageHelper;
      return languageHelper.whenReady();
    }.bind(this));

    teardown(function(done) {
      // Close the section if we're in a sub-page.
      if (settings.getCurrentRoute().isSubpage()) {
        settings.navigateTo(settings.Route.BASIC);
        setTimeout(done);
      } else {
        done();
      }
    });

    suite('add languages dialog', function() {
      var dialog;
      var dialogItems;
      var cancelButton;
      var actionButton;

      setup(function(done) {
        var addLanguagesButton =
            languagesCollapse.querySelector('.list-button:last-of-type');
        MockInteractions.tap(addLanguagesButton);

        // The page stamps the dialog, registers listeners, and populates the
        // iron-list asynchronously at microtask timing, so wait for a new task.
        setTimeout(function() {
          dialog = languagesPage.$$('settings-add-languages-dialog');
          assertTrue(!!dialog);

          actionButton = assert(dialog.$$('.action-button'));
          cancelButton = assert(dialog.$$('.cancel-button'));

          // The fixed-height dialog's iron-list should stamp far fewer than
          // 50 items.
          dialogItems =
              dialog.$.dialog.querySelectorAll('.list-item:not([hidden])');
          assertGT(dialogItems.length, 1);
          assertLT(dialogItems.length, 50);

          // No languages have been checked, so the action button is disabled.
          assertTrue(actionButton.disabled);
          assertFalse(cancelButton.disabled);

          done();
        });
      });

      // After every test, check that the dialog is removed from the DOM.
      teardown(function() {
        Polymer.dom.flush();
        assertEquals(null, languagesPage.$$('settings-add-languages-dialog'));
      });

      test('cancel', function() {
        // Canceling the dialog should close and remove it.
        MockInteractions.tap(cancelButton);
      });

      test('add languages and cancel', function(done) {
        var numEnabled = languagesPage.languages.enabled.length;

        // Check some languages.
        MockInteractions.tap(dialogItems[0]);
        MockInteractions.tap(dialogItems[1]);

        // Canceling the dialog should close and remove it without enabling
        // the checked languages. A small timeout lets us check this.
        MockInteractions.tap(cancelButton);
        setTimeout(function() {
          // Number of enabled languages should be the same.
          assertEquals(numEnabled, languagesPage.languages.enabled.length);
          done();
        }, 100);
      });

      test('add languages and confirm', function() {
        var numEnabled = languagesPage.languages.enabled.length;

        // No languages have been checked, so the action button is inert.
        MockInteractions.tap(actionButton);
        Polymer.dom.flush();
        assertEquals(dialog, languagesPage.$$('settings-add-languages-dialog'));

        // Check and uncheck one language.
        MockInteractions.tap(dialogItems[0]);
        assertFalse(actionButton.disabled);
        MockInteractions.tap(dialogItems[0]);
        assertTrue(actionButton.disabled);

        // Check multiple languages.
        MockInteractions.tap(dialogItems[0]);
        MockInteractions.tap(dialogItems[1]);
        assertFalse(actionButton.disabled);

        // The action button should close and remove the dialog, enabling the
        // checked languages.
        MockInteractions.tap(actionButton);

        // Wait for the languages to be enabled by the browser.
        return whenNumEnabledLanguagesBecomes(numEnabled + 2);
      });
    });

    suite('language menu', function() {
      var origTranslateEnabled;

      suiteSetup(function() {
        // Cache the value of Translate to avoid side effects.
        origTranslateEnabled = languageHelper.prefs.translate.enabled.value;
      });

      suiteTeardown(function() {
        var cur = languageHelper.prefs.translate.enabled.value;
        // Restore the value of Translate.
        languageHelper.setPrefValue('translate.enabled', origTranslateEnabled);
        cur = languageHelper.prefs.translate.enabled.value;
      });

      test('structure', function(done) {
        var languageOptionsDropdownTrigger = languagesCollapse.querySelector(
            'paper-icon-button');
        assertTrue(!!languageOptionsDropdownTrigger);
        MockInteractions.tap(languageOptionsDropdownTrigger);
        var languageMenu = assert(languagesPage.$$('cr-shared-menu'));

        listenOnce(languageMenu, 'iron-overlay-opened', function() {
          assertTrue(languageMenu.menuOpen);

          // Enable Translate so the menu always shows the Translate checkbox.
          languageHelper.setPrefValue('translate.enabled', true);

          var separator = languageMenu.querySelector('hr');
          assertEquals(1, separator.offsetHeight);

          // Disable Translate. On platforms that can't change the UI language,
          // this hides all the checkboxes, so the separator isn't needed.
          // Chrome OS and Windows still show a checkbox and thus the separator.
          languageHelper.setPrefValue('translate.enabled', false);
          if (cr.isChromeOS || cr.isWindows)
            assertEquals(1, separator.offsetHeight);
          else
            assertEquals(0, separator.offsetHeight);

          MockInteractions.tap(languageOptionsDropdownTrigger);
          assertFalse(languageMenu.menuOpen);
          done();
        });
      });

      test('remove language', function() {
        var numEnabled = languagesPage.languages.enabled.length;

        // Enabled a language which we can then disable.
        var newLanguage = getAvailableLanguage();
        languageHelper.enableLanguage(newLanguage.code);

        // Wait for the language to be enabled.
        return whenNumEnabledLanguagesBecomes(numEnabled + 1).then(function() {
          // Populate the dom-repeat.
          Polymer.dom.flush();

          // Find the new language item.
          var items = languagesCollapse.querySelectorAll('.list-item');
          var domRepeat = assert(
              languagesCollapse.querySelector('template[is="dom-repeat"]'));
          var item = Array.from(items).find(function(el) {
            return domRepeat.itemForElement(el) &&
                domRepeat.itemForElement(el).language == newLanguage;
          });

          // Open the menu and select Remove.
          MockInteractions.tap(item.querySelector('paper-icon-button'));

          var languageMenu = assert(languagesPage.$$('cr-shared-menu'));
          assertTrue(languageMenu.menuOpen);
          var removeMenuItem = assert(languageMenu.querySelector(
              '.dropdown-item:last-child'));
          assertFalse(removeMenuItem.disabled);
          MockInteractions.tap(removeMenuItem);
          assertFalse(languageMenu.menuOpen);

          // We should go back down to the original number of enabled languages.
          return whenNumEnabledLanguagesBecomes(numEnabled).then(function() {
            assertFalse(languageHelper.isLanguageEnabled(newLanguage.code));
          });
        });
      });
    });

    test('manage input methods', function() {
      var inputMethodsCollapse = languagesPage.$.inputMethodsCollapse;
      var inputMethodSettingsExist = !!inputMethodsCollapse;
      if (cr.isChromeOS) {
        assertTrue(inputMethodSettingsExist);
        var manageInputMethodsButton =
            inputMethodsCollapse.querySelector('.list-button:last-of-type');
        MockInteractions.tap(manageInputMethodsButton);
        assertTrue(!!languagesPage.$$('settings-manage-input-methods-page'));
      } else {
        assertFalse(inputMethodSettingsExist);
      }
    });

    test('spellcheck', function() {
      var spellCheckCollapse = languagesPage.$.spellCheckCollapse;
      var spellCheckSettingsExist = !!spellCheckCollapse;
      if (cr.isMac) {
        assertFalse(spellCheckSettingsExist);
      } else {
        assertTrue(spellCheckSettingsExist);
        MockInteractions.tap(
            spellCheckCollapse.querySelector('.list-button:last-of-type'));
        assertTrue(!!languagesPage.$$('settings-edit-dictionary-page'));
      }
    });
  }.bind(this));

  // TODO(michaelpg): Test more aspects of the languages UI.

  // Run all registered tests.
  mocha.run();
});
