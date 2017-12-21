// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('languages_page_tests', function() {
  /** @enum {string} */
  const TestNames = {
    AddLanguagesDialog: 'add languages dialog',
    LanguageMenu: 'language menu',
    InputMethods: 'input methods',
    Spellcheck: 'spellcheck',
  };

  suite('languages page', function() {
    /** @type {?LanguageHelper} */
    var languageHelper = null;
    /** @type {?SettingsLanguagesPageElement} */
    var languagesPage = null;
    /** @type {?IronCollapseElement} */
    var languagesCollapse = null;
    /** @type {?CrActionMenuElement} */
    var actionMenu = null;
    /** @type {?settings.LanguagesBrowserProxy} */
    var browserProxy = null;

    // Enabled language pref name for the platform.
    const languagesPref =
        cr.isChromeOS ? 'settings.language.preferred_languages'
                      : 'intl.accept_languages';

    // Initial value of enabled languages pref used in tests.
    const initialLanguages = 'en-US,sw';

    suiteSetup(function() {
      testing.Test.disableAnimationsAndTransitions();
      PolymerTest.clearBody();
      CrSettingsPrefs.deferInitialization = true;
    });

    setup(function() {
      var settingsPrefs = document.createElement('settings-prefs');
      var settingsPrivate =
          new settings.FakeSettingsPrivate(settings.getFakeLanguagePrefs());
      settingsPrefs.initialize(settingsPrivate);
      document.body.appendChild(settingsPrefs);
      return CrSettingsPrefs.initialized.then(function() {
        // Set up test browser proxy.
        browserProxy = new settings.TestLanguagesBrowserProxy();
        settings.LanguagesBrowserProxyImpl.instance_ = browserProxy;

        // Set up fake languageSettingsPrivate API.
        var languageSettingsPrivate = browserProxy.getLanguageSettingsPrivate();
        languageSettingsPrivate.setSettingsPrefs(settingsPrefs);

        languagesPage = document.createElement('settings-languages-page');

        // Prefs would normally be data-bound to settings-languages-page.
        languagesPage.prefs = settingsPrefs.prefs;
        test_util.fakeDataBind(settingsPrefs, languagesPage, 'prefs');

        document.body.appendChild(languagesPage);
        languagesCollapse = languagesPage.$.languagesCollapse;
        languagesCollapse.opened = true;
        actionMenu = languagesPage.$.menu.get();

        languageHelper = languagesPage.languageHelper;
        return languageHelper.whenReady();
      });
    });

    teardown(function() {
      PolymerTest.clearBody();
    });

    suite(TestNames.AddLanguagesDialog, function() {
      var dialog;
      var dialogItems;
      var cancelButton;
      var actionButton;
      var dialogClosedResolver;
      var dialogClosedObserver;

      // Resolves the PromiseResolver if the mutation includes removal of the
      // settings-add-languages-dialog.
      // TODO(michaelpg): Extract into a common method similar to
      // test_util.whenAttributeIs for use elsewhere.
      var onMutation = function(mutations, observer) {
        if (mutations.some(function(mutation) {
          return mutation.type == 'childList' &&
              Array.from(mutation.removedNodes).includes(dialog);
        })) {
          // Sanity check: the dialog should no longer be in the DOM.
          assertEquals(null, languagesPage.$$('settings-add-languages-dialog'));
          observer.disconnect();
          assertTrue(!!dialogClosedResolver);
          dialogClosedResolver.resolve();
        }
      };

      setup(function(done) {
        var addLanguagesButton =
            languagesCollapse.querySelector('#addLanguages');
        MockInteractions.tap(addLanguagesButton);

        // The page stamps the dialog, registers listeners, and populates the
        // iron-list asynchronously at microtask timing, so wait for a new task.
        setTimeout(function() {
          dialog = languagesPage.$$('settings-add-languages-dialog');
          assertTrue(!!dialog);

          // Observe the removal of the dialog via MutationObserver since the
          // HTMLDialogElement 'close' event fires at an unpredictable time.
          dialogClosedResolver = new PromiseResolver();
          dialogClosedObserver = new MutationObserver(onMutation);
          dialogClosedObserver.observe(languagesPage.root, {childList: true});

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

      teardown(function() {
        dialogClosedObserver.disconnect();
      });

      test('cancel', function() {
        // Canceling the dialog should close and remove it.
        MockInteractions.tap(cancelButton);

        return dialogClosedResolver.promise;
      });

      test('add languages and cancel', function() {
        // Check some languages.
        MockInteractions.tap(dialogItems[1]);  // en-CA.
        MockInteractions.tap(dialogItems[2]);  // tk.

        // Canceling the dialog should close and remove it without enabling
        // the checked languages.
        MockInteractions.tap(cancelButton);
        return dialogClosedResolver.promise.then(function() {
          assertEquals(initialLanguages,
                       languageHelper.getPref(languagesPref).value);
        });
      });

      test('add languages and confirm', function() {
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
        MockInteractions.tap(dialogItems[0]);  // en.
        MockInteractions.tap(dialogItems[2]);  // tk.
        assertFalse(actionButton.disabled);

        // The action button should close and remove the dialog, enabling the
        // checked languages.
        MockInteractions.tap(actionButton);

        assertEquals(
            initialLanguages + ',en,tk',
            languageHelper.getPref(languagesPref).value);

        return dialogClosedResolver.promise;
      });

      // Test that searching languages works whether the displayed or native
      // language name is queried.
      test('search languages', function() {
        var searchInput = dialog.$$('settings-subpage-search');

        var getItems = function() {
          return dialog.$.dialog.querySelectorAll('.list-item:not([hidden])');
        };

        // Expecting a few languages to be displayed when no query exists.
        assertGE(getItems().length, 1);

        // Issue query that matches the |displayedName|.
        searchInput.setValue('greek');
        Polymer.dom.flush();
        assertEquals(1, getItems().length);

        // Issue query that matches the |nativeDisplayedName|.
        searchInput.setValue('Ελληνικά');
        Polymer.dom.flush();
        assertEquals(1, getItems().length);

        // Issue query that does not match any language.
        searchInput.setValue('egaugnal');
        Polymer.dom.flush();
        assertEquals(0, getItems().length);
      });
    });

    suite(TestNames.LanguageMenu, function() {
      /*
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

      /*
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

      test('structure', function() {
        var languageOptionsDropdownTrigger = languagesCollapse.querySelector(
            'button');
        assertTrue(!!languageOptionsDropdownTrigger);
        MockInteractions.tap(languageOptionsDropdownTrigger);
        assertTrue(actionMenu.open);

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
        var settingsToggle = languagesPage.$.offerTranslateOtherLanguages;
        assertTrue(!!settingsToggle);
        assertTrue(!!settingsToggle);

        // Clicking on the toggle switches it to false.
        MockInteractions.tap(settingsToggle);
        var newToggleValue = languageHelper.prefs.translate.enabled.value;
        assertFalse(newToggleValue);

        // Clicking on the toggle switches it to true again.
        MockInteractions.tap(settingsToggle);
        newToggleValue = languageHelper.prefs.translate.enabled.value;
        assertTrue(newToggleValue);
      });

      test('toggle translate for a specific language', function(done) {
        // Open options for 'sw'.
        var languageOptionsDropdownTrigger =
            languagesCollapse.querySelectorAll('button')[1];
        assertTrue(!!languageOptionsDropdownTrigger);
        MockInteractions.tap(languageOptionsDropdownTrigger);
        assertTrue(actionMenu.open);

        // 'sw' supports translate to the target language ('en').
        var translateOption = getMenuItem('offerToTranslateInThisLanguage');
        assertFalse(translateOption.disabled);
        assertTrue(translateOption.checked);

        // Toggle the translate option.
        MockInteractions.tap(translateOption);

        // Menu should stay open briefly.
        assertTrue(actionMenu.open);
        // Guaranteed to run later than the menu close delay.
        setTimeout(function() {
          assertFalse(actionMenu.open);
          assertDeepEquals(
              ['en-US', 'sw'],
              languageHelper.prefs.translate_blocked_languages.value);
          done();
        }, settings.kMenuCloseDelay + 1);
      });

      test('disable translate hides language-specific option', function() {
        // Disables translate.
        languageHelper.setPrefValue('translate.enabled', false);

        // Open options for 'sw'.
        var languageOptionsDropdownTrigger =
            languagesCollapse.querySelectorAll('button')[1];
        assertTrue(!!languageOptionsDropdownTrigger);
        MockInteractions.tap(languageOptionsDropdownTrigger);
        assertTrue(actionMenu.open);

        // The language-specific translation option should be hidden.
        var translateOption = actionMenu.querySelector('#offerTranslations');
        assertTrue(!!translateOption);
        assertTrue(translateOption.hidden);
      });

      test('remove language', function() {
        // Enable a language which we can then disable.
        languageHelper.enableLanguage('no');

        // Populate the dom-repeat.
        Polymer.dom.flush();

        // Find the new language item.
        var items = languagesCollapse.querySelectorAll('.list-item');
        var domRepeat = assert(
            languagesCollapse.querySelector('template[is="dom-repeat"]'));
        var item = Array.from(items).find(function(el) {
          return domRepeat.itemForElement(el) &&
              domRepeat.itemForElement(el).language.code == 'no';
        });

        // Open the menu and select Remove.
        MockInteractions.tap(item.querySelector('button'));

        assertTrue(actionMenu.open);
        var removeMenuItem = getMenuItem('removeLanguage');
        assertFalse(removeMenuItem.disabled);
        MockInteractions.tap(removeMenuItem);
        assertFalse(actionMenu.open);

        assertEquals(
            initialLanguages, languageHelper.getPref(languagesPref).value);
      });

      test('move up/down buttons', function() {
        // Add several languages.
        for (var language of ['en-CA', 'en-US', 'tk', 'no'])
          languageHelper.enableLanguage(language);

        Polymer.dom.flush();

        var menuButtons =
            languagesCollapse.querySelectorAll(
                '.list-item button.icon-more-vert');

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

    test(TestNames.InputMethods, function() {
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

    test(TestNames.Spellcheck, function() {
      var spellCheckCollapse = languagesPage.$.spellCheckCollapse;
      var spellCheckSettingsExist = !!spellCheckCollapse;
      if (cr.isMac) {
        assertFalse(spellCheckSettingsExist);
      } else {
        assertTrue(spellCheckSettingsExist);

        // The row button should have a secondary row specifying which language
        // spell check is enabled for.
        var triggerRow = languagesPage.$.spellCheckSubpageTrigger;

        // en-US starts with spellcheck enabled, so the secondary row is
        // populated.
        assertTrue(triggerRow.classList.contains('two-line'));
        assertLT(
            0, triggerRow.querySelector('.secondary').textContent.length);

        MockInteractions.tap(triggerRow);
        Polymer.dom.flush();

        // Disable spellcheck for en-US.
        var spellcheckLanguageToggle =
            spellCheckCollapse.querySelector('cr-toggle[checked]');
        assertTrue(!!spellcheckLanguageToggle);
        MockInteractions.tap(spellcheckLanguageToggle);
        assertFalse(spellcheckLanguageToggle.checked);
        assertEquals(
            0,
            languageHelper.prefs.spellcheck.dictionaries.value.length);

        // Now the secondary row is empty, so it shouldn't be shown.
        assertFalse(triggerRow.classList.contains('two-line'));
        assertEquals(
            0, triggerRow.querySelector('.secondary').textContent.length);

        // Force-enable a language via policy.
        languageHelper.setPrefValue('spellcheck.forced_dictionaries', ['nb']);

        // The second row should no longer be empty.
        assertTrue(triggerRow.classList.contains('two-line'));
        assertLT(
            0, triggerRow.querySelector('.secondary').textContent.length);

        // Force-disable spellchecking via policy.
        languageHelper.setPrefValue('browser.enable_spellchecking', false);
        Polymer.dom.flush();

        // The second row should not be empty.
        assertTrue(triggerRow.classList.contains('two-line'));
        assertLT(0, triggerRow.querySelector('.secondary').textContent.length);

        // The policy indicator should be present.
        assertTrue(!!triggerRow.querySelector('cr-policy-pref-indicator'));

        // Force-enable spellchecking via policy, and ensure that the policy
        // indicator is not present. |enable_spellchecking| can be forced to
        // true by policy, but no indicator should be shown in that case.
        languageHelper.setPrefValue('browser.enable_spellchecking', true);
        Polymer.dom.flush();
        assertFalse(!!triggerRow.querySelector('cr-policy-pref-indicator'));
      }
    });
  });

  return {TestNames: TestNames};
});
