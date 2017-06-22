// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_people_page_quick_unlock', function() {
  var element = null;
  var quickUnlockPrivateApi = null;
  var QuickUnlockMode = chrome.quickUnlockPrivate.QuickUnlockMode;
  var fakeUma = null;

  /**
   * Returns if the element is visible.
   * @param {!Element} element
   */
  function isVisible(element) {
    while (element) {
      if (element.offsetWidth <= 0 || element.offsetHeight <= 0 ||
          element.hidden ||
          window.getComputedStyle(element).visibility == 'hidden') {
        return false;
      }

      element = element.parentElement;
    }

    return true;
  }

  /**
   * Returns true if the given |element| has class |className|.
   * @param {!Element} element
   * @param {string} className
   */
  function assertHasClass(element, className) {
    assertTrue(element.classList.contains(className));
  }

  /**
   * Returns the result of running |selector| on element.
   * @param {string} selector
   * @return {Element}
   */
  function getFromElement(selector) {
    var childElement = element.$$(selector);
    assertTrue(!!childElement);
    return childElement;
  }

  /**
   * Sets the active quick unlock modes and raises a mode change event.
   * @param {!Array<chrome.quickUnlockPrivate.QuickUnlockMode>} modes
   */
  function setActiveModes(modes) {
    quickUnlockPrivateApi.activeModes = modes;
    quickUnlockPrivateApi.onActiveModesChanged.callListeners(modes);
  }

  function registerAuthenticateTests() {
    suite('authenticate', function() {
      var passwordElement = null;

      setup(function() {
        PolymerTest.clearBody();

        quickUnlockPrivateApi = new settings.FakeQuickUnlockPrivate();
        fakeUma = new settings.FakeQuickUnlockUma();

        element = document.createElement('settings-password-prompt-dialog');
        element.quickUnlockPrivate_ = quickUnlockPrivateApi;
        element.writeUma_ = fakeUma.recordProgress.bind(fakeUma);
        document.body.appendChild(element);

        passwordElement = getFromElement('#passwordInput');
      });

      test('PasswordCheckDoesNotChangeActiveMode', function() {
        // No active modes.
        quickUnlockPrivateApi.activeModes = [];
        passwordElement.value = 'foo';
        element.submitPassword_();
        assertDeepEquals([], quickUnlockPrivateApi.activeModes);
        assertDeepEquals([], quickUnlockPrivateApi.credentials);

        // PIN is active.
        quickUnlockPrivateApi.activeModes = [QuickUnlockMode.PIN];
        passwordElement.value = 'foo';
        element.submitPassword_();
        assertDeepEquals([QuickUnlockMode.PIN],
                         quickUnlockPrivateApi.activeModes);
        assertDeepEquals([''], quickUnlockPrivateApi.credentials);
      });

      // A bad password does not provide an authenticated setModes object, and a
      // entered password correctly uma should not be recorded.
      test('InvalidPasswordDoesNotProvideAuthentication', function() {
        quickUnlockPrivateApi.accountPassword = 'bar';

        passwordElement.value = 'foo';
        element.submitPassword_();

        assertEquals(0, fakeUma.getHistogramValue(
            LockScreenProgress.ENTER_PASSWORD_CORRECTLY));
        assertFalse(!!element.setModes);
      });

      // A valid password provides an authenticated setModes object, and a
      // entered password correctly uma should be recorded.
      test('ValidPasswordProvidesAuthentication', function() {
        quickUnlockPrivateApi.accountPassword = 'foo';

        passwordElement.value = 'foo';
        element.submitPassword_();

        assertEquals(1, fakeUma.getHistogramValue(
            LockScreenProgress.ENTER_PASSWORD_CORRECTLY));
        assertTrue(!!element.setModes);
      });

      // The setModes objects times out after a delay.
      test('AuthenticationTimesOut', function(done) {
        quickUnlockPrivateApi.accountPassword = 'foo';

        element.passwordActiveDurationMs_ = 0;
        passwordElement.value = 'foo';
        element.submitPassword_();

        assertFalse(!!element.password_);
        assertTrue(!!element.setModes);

        // Two setTimeout calls with the same delay are guaranteed to execute in
        // the same order that they were submitted in, so using
        // element.autosubmitDelayMs_ is safe.
        setTimeout(function() {
          assertFalse(!!element.password_);
          assertFalse(!!element.setModes);
          done();
        }, element.passwordActiveDurationMs_);
      });
    });
  }

  function registerLockScreenTests() {
    suite('lock-screen', function() {
      /** @const */ var ENABLE_LOCK_SCREEN_PREF = 'settings.enable_screen_lock';

      var fakeSettings = null;
      var passwordRadioButton = null;
      var pinPasswordRadioButton = null;
      var noneRadioButton = null;

      /**
       * Asserts that only the given radio button is active and all of the
       * others are inactive.
       * @param {Element} radioButton
       */
      function assertRadioButtonActive(radioButton) {
        function doAssert(element, name) {
          if (radioButton == element)
            assertTrue(element.active, 'Expected ' + name + ' to be active');
          else
            assertFalse(element.active, 'Expected ' + name + ' to be inactive');
        }

        doAssert(passwordRadioButton, 'passwordButton');
        doAssert(pinPasswordRadioButton, 'pinPasswordButton');
      }

      /**
       * Returns the lock screen pref value.
       * @return {boolean}
       */
      function getLockScreenPref() {
        var result;
        fakeSettings.getPref(ENABLE_LOCK_SCREEN_PREF, function(value) {
          result = value;
        });
        assertNotEquals(undefined, result);
        return result.value;
      }

      /**
       * Changes the lock screen pref value using the settings API; this is like
       * the pref got changed from an unkown source such as another tab.
       * @param {boolean} value
       */
      function setLockScreenPref(value) {
        fakeSettings.setPref(ENABLE_LOCK_SCREEN_PREF, value, '', assertTrue);
      }

      function isSetupPinButtonVisible() {
        Polymer.dom.flush();
        var setupPinButton = element.$$('#setupPinButton');
        return isVisible(setupPinButton);
      }

      setup(function(done) {
        PolymerTest.clearBody();

        CrSettingsPrefs.deferInitialization = true;

        // Build pref fakes.
        var fakePrefs = [{
          key: ENABLE_LOCK_SCREEN_PREF,
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true
        }];
        fakeSettings = new settings.FakeSettingsPrivate(fakePrefs);
        fakeUma = new settings.FakeQuickUnlockUma();
        setLockScreenPref(true);
        var prefElement = document.createElement('settings-prefs');
        prefElement.initialize(fakeSettings);
        document.body.appendChild(prefElement);

        // Wait for prefElement to finish initializing; it takes some time for
        // the prefs element to get allocated.
        prefElement.addEventListener('prefs-changed', function prefsReady() {
          prefElement.removeEventListener('prefs-changed', prefsReady);

          quickUnlockPrivateApi = new settings.FakeQuickUnlockPrivate();

          // Create choose-method element.
          element = document.createElement('settings-lock-screen');
          element.settingsPrivate_ = fakeSettings;
          element.quickUnlockPrivate_ = quickUnlockPrivateApi;
          element.prefs = prefElement.prefs;
          element.writeUma_ = fakeUma.recordProgress.bind(fakeUma);

          document.body.appendChild(element);
          Polymer.dom.flush();

          element.setModes_ =
              quickUnlockPrivateApi.setModes.bind(quickUnlockPrivateApi, '');

          passwordRadioButton =
              getFromElement('paper-radio-button[name="password"]');
          pinPasswordRadioButton =
              getFromElement('paper-radio-button[name="pin+password"]');

          done();
        });
      });

      // Showing the choose method screen does not make any destructive pref or
      // quickUnlockPrivate calls.
      test('ShowingScreenDoesNotModifyPrefs', function() {
        assertTrue(getLockScreenPref());
        assertRadioButtonActive(passwordRadioButton);
        assertDeepEquals([], quickUnlockPrivateApi.activeModes);
      });

      // The various radio buttons update internal state and do not modify
      // prefs.
      test('TappingButtonsChangesUnderlyingState', function() {
        function togglePin() {
          assertRadioButtonActive(passwordRadioButton);

          // Tap pin+password button.
          MockInteractions.tap(pinPasswordRadioButton);
          assertRadioButtonActive(pinPasswordRadioButton);
          assertTrue(isSetupPinButtonVisible());
          assertDeepEquals([], quickUnlockPrivateApi.activeModes);

          // Enable quick unlock so that we verify tapping password disables it.
          setActiveModes([QuickUnlockMode.PIN]);

          // Tap password button and verify quick unlock is disabled.
          MockInteractions.tap(passwordRadioButton);
          assertRadioButtonActive(passwordRadioButton);
          assertFalse(isSetupPinButtonVisible());
          assertDeepEquals([], quickUnlockPrivateApi.activeModes);
        }

        // Verify toggling PIN on/off does not disable screen lock.
        setLockScreenPref(true);
        togglePin();
        assertTrue(getLockScreenPref());

        // Verify toggling PIN on/off does not enable screen lock.
        setLockScreenPref(false);
        togglePin();
        assertFalse(getLockScreenPref());
      });

      // If quick unlock is changed by another settings page the radio button
      // will update to show quick unlock is active.
      test('EnablingQuickUnlockChangesButtonState', function() {
        setActiveModes([QuickUnlockMode.PIN]);
        assertRadioButtonActive(pinPasswordRadioButton);
        assertTrue(isSetupPinButtonVisible());

        setActiveModes([]);
        assertRadioButtonActive(passwordRadioButton);
        assertDeepEquals([], quickUnlockPrivateApi.activeModes);
      });

      // Tapping the PIN configure button opens up the setup PIN dialog, and
      // records a chose pin or password uma.
      test('TappingConfigureOpensSetupPin', function() {
        assertEquals(0, fakeUma.getHistogramValue(
            LockScreenProgress.CHOOSE_PIN_OR_PASSWORD));
        assertRadioButtonActive(passwordRadioButton);

        MockInteractions.tap(pinPasswordRadioButton);
        assertTrue(isSetupPinButtonVisible());
        assertRadioButtonActive(pinPasswordRadioButton)

        Polymer.dom.flush();
        MockInteractions.tap(getFromElement('#setupPinButton'));
        Polymer.dom.flush();
        var setupPinDialog = getFromElement('#setupPin');
        assertTrue(setupPinDialog.$$('#dialog').open);
        assertEquals(1, fakeUma.getHistogramValue(
            LockScreenProgress.CHOOSE_PIN_OR_PASSWORD));
      });
    });
  }

  function registerSetupPinDialogTests() {
    suite('setup-pin-dialog', function() {
      var titleDiv = null;
      var problemDiv = null;
      var pinKeyboard = null;
      var backButton = null;
      var continueButton = null;

      setup(function() {
        PolymerTest.clearBody();

        quickUnlockPrivateApi = new settings.FakeQuickUnlockPrivate();
        fakeUma = new settings.FakeQuickUnlockUma();

        // Create setup-pin element.
        element = document.createElement('settings-setup-pin-dialog');
        element.quickUnlockPrivate_ = quickUnlockPrivateApi;
        element.setModes =
            quickUnlockPrivateApi.setModes.bind(quickUnlockPrivateApi, '');
        element.writeUma_ = fakeUma.recordProgress.bind(fakeUma);

        document.body.appendChild(element);
        Polymer.dom.flush();

        titleDiv = getFromElement('div[class="title"]');
        problemDiv = getFromElement('#problemDiv');
        pinKeyboard = getFromElement('pin-keyboard');
        backButton = getFromElement('paper-button[class="cancel-button"]');
        continueButton = getFromElement('paper-button[class="action-button"]');

        assertTrue(isVisible(backButton));
        assertTrue(isVisible(continueButton));
      });

      // The continue button and title change text between the setup and confirm
      // steps.
      test('TextChangesBetweenSetupAndConfirmStep', function() {
        var initialContinue = continueButton.textContent;
        var initialTitle = titleDiv.textContent;

        pinKeyboard.value = '1111';
        MockInteractions.tap(continueButton);

        assertNotEquals(initialContinue, continueButton.textContent);
        assertNotEquals(initialTitle, titleDiv.textContent);
      });

      // The continue button is disabled unless the user has entered a >= 4
      // digit PIN.
      test('CanOnlyContinueAfterEnteringAtLeastFourDigitPin', function() {
        pinKeyboard.value = '111';
        assertTrue(continueButton.disabled);

        pinKeyboard.value = '1111';
        assertFalse(continueButton.disabled);

        pinKeyboard.value = '111';
        assertTrue(continueButton.disabled);

        pinKeyboard.value = '';
        assertTrue(continueButton.disabled);

        pinKeyboard.value = '1111111';
        assertFalse(continueButton.disabled);
      });

      // Problem message is always shown.
      test('ProblemShownEvenWithEmptyPin', function() {
        pinKeyboard.value = '11';
        assertTrue(isVisible(problemDiv));

        pinKeyboard.value = '';
        assertTrue(isVisible(problemDiv));
      });

      // If the PIN is too short a problem is shown.
      test('WarningShownForShortPins', function() {
        // Verify initially when the PIN is less than 4 digits, the problem will
        // be a warning.
        pinKeyboard.value = '';
        assertTrue(isVisible(problemDiv));
        assertHasClass(problemDiv, 'warning');
        assertTrue(continueButton.disabled);

        // Verify that once the PIN is 4 digits (do not use 1111 since that will
        // bring up a easy to guess warning) the warning disappears.
        pinKeyboard.value = '1321';
        assertFalse(isVisible(problemDiv));
        assertFalse(continueButton.disabled);

        // Verify that after we pass 4 digits once, but delete some digits, the
        // problem will be an error.
        pinKeyboard.value = '11';
        assertHasClass(problemDiv, 'error');
        assertTrue(continueButton.disabled);
      });

      // If the PIN is too long an error problem is shown.
      test('WarningShownForLongPins', function() {
        // By default, there is no max length on pins.
        quickUnlockPrivateApi.credentialRequirements.maxLength = 5;

        // A pin of length five should be valid when max length is five.
        pinKeyboard.value = '11111';
        assertFalse(isVisible(problemDiv));
        assertFalse(continueButton.disabled);

        // A pin of length six should not be valid when max length is five.
        pinKeyboard.value = '111111';
        assertTrue(isVisible(problemDiv));
        assertHasClass(problemDiv, 'error');
        assertTrue(continueButton.disabled);
      });

      // If the PIN is weak a warning problem is shown.
      test('WarningShownForWeakPins', function() {
        pinKeyboard.value = '1111';

        assertTrue(isVisible(problemDiv));
        assertHasClass(problemDiv, 'warning');
      });

      // Show a error if the user tries to submit a PIN that does not match the
      // initial PIN. The error disappears once the user edits the wrong PIN.
      test('WarningThenErrorShownForMismatchedPins', function() {
        pinKeyboard.value = '1118';
        MockInteractions.tap(continueButton);

        // Entering a mismatched PIN shows a warning.
        pinKeyboard.value = '1119';
        assertFalse(isVisible(problemDiv));

        // Submitting a mistmatched PIN shows an error. Directly call the button
        // event since a tap on the disabled button does nothing.
        element.onPinSubmit_();
        assertHasClass(problemDiv, 'error');

        // Changing the PIN changes the error to a warning.
        pinKeyboard.value = '111';
        assertFalse(isVisible(problemDiv));
      });

      // Hitting cancel at the setup step dismisses the dialog.
      test('HittingBackButtonResetsState', function() {
        MockInteractions.tap(backButton);
        assertFalse(element.$$('#dialog').open);
      });

      // Hitting cancel at the confirm step dismisses the dialog.
      test('HittingBackButtonResetsState', function() {
        pinKeyboard.value = '1111';
        MockInteractions.tap(continueButton);
        MockInteractions.tap(backButton);
        assertFalse(element.$$('#dialog').open);
      });

      // User has to re-enter PIN for confirm step.
      test('PinKeyboardIsResetForConfirmStep', function() {
        pinKeyboard.value = '1111';
        MockInteractions.tap(continueButton);
        assertEquals('', pinKeyboard.value);
      });

      // Completing the flow results in a call to the quick unlock private API.
      // Check that uma stats are called as expected.
      test('SubmittingPinCallsQuickUnlockApi', function() {
        // Entering the same (even weak) pin twice calls the quick unlock API
        // and sets up a PIN.
        assertEquals(0, fakeUma.getHistogramValue(
            LockScreenProgress.ENTER_PIN));
        assertEquals(0, fakeUma.getHistogramValue(
            LockScreenProgress.CONFIRM_PIN));
        pinKeyboard.value = '1111';
        MockInteractions.tap(continueButton);
        assertEquals(1, fakeUma.getHistogramValue(
            LockScreenProgress.ENTER_PIN));

        pinKeyboard.value = '1111';
        MockInteractions.tap(continueButton);

        assertEquals(1, fakeUma.getHistogramValue(
            LockScreenProgress.CONFIRM_PIN));
        assertDeepEquals(['PIN'], quickUnlockPrivateApi.activeModes);
        assertDeepEquals(['1111'], quickUnlockPrivateApi.credentials);
      });
    });
  }

  return {
    registerAuthenticateTests: registerAuthenticateTests,
    registerLockScreenTests: registerLockScreenTests,
    registerSetupPinDialogTests: registerSetupPinDialogTests
  };
});
