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
    var page = this.getPage('basic');

    var languagesSection;
    var languagesPage;
    var languagesCollapse;
    var languageHelper;
    var actionMenu;

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

    // Returns supported languages that are not enabled.
    function getAvailableLanguages() {
      return languagesPage.languages.supported.filter(function(language) {
        return !languageHelper.isLanguageEnabled(language.code);
      });
    }

    suiteSetup(function() {
      page.set('pageVisibility.languages', true);
      Polymer.dom.flush();

      languagesSection = assert(this.getSection(page, 'languages'));
      languagesPage = assert(
          languagesSection.querySelector('settings-languages-page'));
      languagesCollapse = languagesPage.$.languagesCollapse;
      languagesCollapse.opened = true;
      actionMenu = languagesPage.$.menu.get();

      languageHelper = languagesPage.languageHelper;
      return languageHelper.whenReady();
    }.bind(this));

    teardown(function(done) {
      if (actionMenu.open)
        actionMenu.close();

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
            languagesCollapse.querySelector('#addLanguages');
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

      /**
       * Finds, asserts and returns the menu item for the given i18n key.
       * @param {string} i18nKey Name of the i18n string for the item's text.
       * @return {!HTMLElement} Menu item.
       */
      function getMenuItem(i18nKey) {
        var i18nString = assert(loadTimeData.getString(i18nKey));
        var menuItems = actionMenu.querySelectorAll('.dropdown-item');
        var menuItem = Array.from(menuItems).find(
            item => item.textContent.trim() == i18nString);
        return assert(menuItem, 'Menu item "' + i18nKey + '" not found');
      }

      /**
       * Checks the visibility of each expected menu item button.
       * param {!Object<boolean>} Dictionary from i18n keys to expected
       *     visibility of those menu items.
       */
      function assertMenuItemButtonsVisible(buttonVisibility) {
        assertTrue(actionMenu.open);
        for (var buttonKey of Object.keys(buttonVisibility)) {
          var buttonItem = getMenuItem(buttonKey);
          assertEquals(!buttonVisibility[buttonKey], buttonItem.hidden,
                       'Menu item "' + buttonKey + '" hidden');
        }
      }

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

      test('structure', function() {
        var languageOptionsDropdownTrigger = languagesCollapse.querySelector(
            'paper-icon-button');
        assertTrue(!!languageOptionsDropdownTrigger);
        MockInteractions.tap(languageOptionsDropdownTrigger);
        assertTrue(actionMenu.open);

        // Enable Translate so the menu always shows the Translate checkbox.
        languageHelper.setPrefValue('translate.enabled', true);

        var separator = actionMenu.querySelector('hr');
        assertEquals(1, separator.offsetHeight);

        // Disable Translate. On platforms that can't change the UI language,
        // this hides all the checkboxes, so the separator isn't needed.
        // Chrome OS and Windows still show a checkbox and thus the separator.
        languageHelper.setPrefValue('translate.enabled', false);
        assertEquals(
            cr.isChromeOS || cr.isWindows ? 1 : 0, separator.offsetHeight);
      });

      test('test translate.enable toggle', function() {
        languageHelper.setPrefValue('translate.enabled', true);
        var toggle = languagesPage.$.offerTranslateOtherLangs.root
            .querySelectorAll('paper-toggle-button')[0];
        assertTrue(!!toggle);

        // Clicking on toggle switch it to false.
        MockInteractions.tap(toggle);
        var newToggleValue = languageHelper.prefs.translate.enabled.value;
        assertFalse(newToggleValue);

        // Clicking on toggle switch it to true again.
        MockInteractions.tap(toggle);
        newToggleValue = languageHelper.prefs.translate.enabled.value;
        assertTrue(newToggleValue);
      });

      test('toggle translate for a specific language', function(done) {
        // Enable Translate so the menu always shows the Translate checkbox.
        languageHelper.setPrefValue('translate.enabled', true);
        languagesPage.set('languages.translateTarget', 'foo');
        languagesPage.set('languages.enabled.1.supportsTranslate', true);

        var languageOptionsDropdownTrigger =
            languagesCollapse.querySelectorAll('paper-icon-button')[1];
        assertTrue(!!languageOptionsDropdownTrigger);
        MockInteractions.tap(languageOptionsDropdownTrigger);
        assertTrue(actionMenu.open);

        // Toggle the translate option.
        var translateOption = getMenuItem('offerToTranslateInThisLanguage');
        assertFalse(translateOption.disabled);
        MockInteractions.tap(translateOption);

        // Menu should stay open briefly.
        assertTrue(actionMenu.open);
        // Guaranteed to run later than the menu close delay.
        setTimeout(function() {
          assertFalse(actionMenu.open);
          done();
        }, settings.kMenuCloseDelay + 1);
      });

      test('disable translate hides language-specific option', function() {
        // Disables translate.
        languageHelper.setPrefValue('translate.enabled', false);
        languagesPage.set('languages.translateTarget', 'foo');
        languagesPage.set('languages.enabled.1.supportsTranslate', true);

        // Makes sure language-specific menu exists.
        var languageOptionsDropdownTrigger =
            languagesCollapse.querySelectorAll('paper-icon-button')[1];
        assertTrue(!!languageOptionsDropdownTrigger);
        MockInteractions.tap(languageOptionsDropdownTrigger);
        assertTrue(actionMenu.open);

        // The language-specific translation option should be hidden.
        var translateOption = actionMenu.querySelector('#offerTranslations');
        assertTrue(!!translateOption);
        assertTrue(translateOption.hidden);
      });

      test('remove language', function() {
        var numEnabled = languagesPage.languages.enabled.length;

        // Enabled a language which we can then disable.
        var newLanguage = assert(getAvailableLanguages()[0]);
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

          assertTrue(actionMenu.open);
          var removeMenuItem = getMenuItem('removeLanguage');
          assertFalse(removeMenuItem.disabled);
          MockInteractions.tap(removeMenuItem);
          assertFalse(actionMenu.open);

          // We should go back down to the original number of enabled languages.
          return whenNumEnabledLanguagesBecomes(numEnabled);
        }).then(function() {
          assertFalse(languageHelper.isLanguageEnabled(newLanguage.code));
        });
      });

      test('move up/down buttons', function() {
        // Add several languages.
        var numEnabled = languagesPage.languages.enabled.length;
        var available = getAvailableLanguages();
        for (var i = 0; i < 4; i++)
          languageHelper.enableLanguage(assert(available[i]).code);

        return whenNumEnabledLanguagesBecomes(numEnabled + 4).then(function() {
          Polymer.dom.flush();

          var menuButtons =
              languagesCollapse.querySelectorAll(
                  '.list-item paper-icon-button[icon="cr:more-vert"]');

          // First language should not have "Move up" or "Move to top".
          MockInteractions.tap(menuButtons[0]);
          assertMenuItemButtonsVisible({
            moveToTop: false, moveUp: false, moveDown: true,
          });
          actionMenu.close();

          // Second language should not have "Move up".
          MockInteractions.tap(menuButtons[1]);
          assertMenuItemButtonsVisible({
            moveToTop: true, moveUp: false, moveDown: true,
          });
          actionMenu.close();

          // Middle languages should have all buttons.
          MockInteractions.tap(menuButtons[2]);
          assertMenuItemButtonsVisible({
            moveToTop: true, moveUp: true, moveDown: true,
          });
          actionMenu.close();

          // Last language should not have "Move down".
          MockInteractions.tap(menuButtons[menuButtons.length - 1]);
          assertMenuItemButtonsVisible({
            moveToTop: true, moveUp: true, moveDown: false,
          });
          actionMenu.close();
        });
      });
    });

    test('manage input methods', function() {
      var inputMethodsCollapse = languagesPage.$.inputMethodsCollapse;
      var inputMethodSettingsExist = !!inputMethodsCollapse;
      if (cr.isChromeOS) {
        assertTrue(inputMethodSettingsExist);
        var manageInputMethodsButton =
            inputMethodsCollapse.querySelector('#manageInputMethods');
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

        // Ensure no language has spell check enabled.
        for (var i = 0; i < languagesPage.languages.enabled.length; i++) {
          languagesPage.set(
              'languages.enabled.' + i + '.spellCheckEnabled', false);
        }

        // The row button should have the extra row only if some language has
        // spell check enabled.
        var triggerRow = languagesPage.$.spellCheckSubpageTrigger;
        assertFalse(triggerRow.classList.contains('two-line'));
        assertEquals(
            0, triggerRow.querySelector('.secondary').textContent.length);

        languagesPage.set(
            'languages.enabled.0.language.supportsSpellcheck', true);
        languagesPage.set('languages.enabled.0.spellCheckEnabled', true);
        assertTrue(triggerRow.classList.contains('two-line'));
        assertLT(
            0, triggerRow.querySelector('.secondary').textContent.length);
      }
    });
  }.bind(this));

  // TODO(michaelpg): Test more aspects of the languages UI.

  // Run all registered tests.
  mocha.run();
});
