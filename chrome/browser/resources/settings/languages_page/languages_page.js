// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-languages-page' is the settings page
 * for language and input method settings.
 */
cr.exportPath('settings');

/**
 * @const {number} Millisecond delay that can be used when closing an action
 *      menu to keep it briefly on-screen.
 */
settings.kMenuCloseDelay = 100;

/**
 * Name of the language setting is shown uma histogram.
 * @type {string}
 */
const LANGUAGE_SETTING_IS_SHOWN_UMA_NAME = 'Translate.LanguageSettingsIsShown';

(function() {
'use strict';

Polymer({
  is: 'settings-languages-page',

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Read-only reference to the languages model provided by the
     * 'settings-languages' instance.
     * @type {!LanguagesModel|undefined}
     */
    languages: {
      type: Object,
      notify: true,
    },

    /** @type {!LanguageHelper} */
    languageHelper: Object,

    // <if expr="not is_macosx">
    /** @private */
    spellCheckSecondaryText_: {
      type: String,
      value: '',
      computed: 'getSpellCheckSecondaryText_(languages.enabled.*)',
    },
    // </if>

    /**
     * The language to display the details for.
     * @type {!LanguageState|undefined}
     * @private
     */
    detailLanguage_: Object,

    /**
     * Whether the language settings list is opened.
     * @private
     */
    languagesOpened_: {
      type: Boolean,
      observer: 'onLanguagesOpenedChanged_',
    },

    /** @private */
    showAddLanguagesDialog_: Boolean,

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value: function() {
        var map = new Map();
        // <if expr="not is_macosx">
        if (settings.routes.EDIT_DICTIONARY) {
          map.set(
              settings.routes.EDIT_DICTIONARY.path,
              '#spellCheckCollapse .subpage-arrow');
        }
        // </if>
        // <if expr="chromeos">
        if (settings.routes.INPUT_METHODS) {
          map.set(
              settings.routes.INPUT_METHODS.path,
              '#inputMethodsCollapse .subpage-arrow');
        }
        // </if>
        return map;
      },
    },
  },

  /**
   * Stamps and opens the Add Languages dialog, registering a listener to
   * disable the dialog's dom-if again on close.
   * @param {!Event} e
   * @private
   */
  onAddLanguagesTap_: function(e) {
    e.preventDefault();
    this.showAddLanguagesDialog_ = true;
    this.async(function() {
      var dialog = this.$$('settings-add-languages-dialog');
      dialog.addEventListener('close', () => {
        this.showAddLanguagesDialog_ = false;
        cr.ui.focusWithoutInk(assert(this.$.addLanguages));
      });
    });
  },

  /**
   * Used to determine which "Move" buttons to show for ordering enabled
   * languages.
   * @param {number} n
   * @param {!LanguageState} language
   * @return {boolean} True if |language| is at the |n|th index in the list of
   *     enabled languages.
   * @private
   */
  isNthLanguage_: function(n, language) {
    var compareLanguage = assert(this.languages.enabled[n]);
    return language.language == compareLanguage.language;
  },

  /**
   * @param {!LanguageState} language
   * @return {boolean} True if the "Move to top" option for |language| should be
   *     visible.
   * @private
   */
  showMoveUp_: function(language) {
    // "Move up" is a no-op for the top language, and redundant with
    // "Move to top" for the 2nd language.
    return !this.isNthLanguage_(0, language) &&
        !this.isNthLanguage_(1, language);
  },

  /**
   * @param {!LanguageState} language
   * @return {boolean} True if the "Move down" option for |language| should be
   *     visible.
   * @private
   */
  showMoveDown_: function(language) {
    return !this.isNthLanguage_(this.languages.enabled.length - 1, language);
  },

  /**
   * @param {!Object} change Polymer change object for languages.enabled.*.
   * @return {boolean} True if there are less than 2 languages.
   */
  isHelpTextHidden_: function(change) {
    return this.languages.enabled.length <= 1;
  },

  // <if expr="chromeos">
  /**
   * Applies Chrome OS session tweaks to the menu.
   * @param {!CrActionMenuElement} menu
   * @private
   */
  tweakMenuForCrOS_: function(menu) {
    // In a CrOS multi-user session, the primary user controls the UI language.
    // TODO(michaelpg): The language selection should not be hidden, but should
    // show a policy indicator. crbug.com/648498
    if (this.isSecondaryUser_())
      menu.querySelector('#uiLanguageItem').hidden = true;

    // The UI language choice doesn't persist for guests.
    if (loadTimeData.getBoolean('isGuest'))
      menu.querySelector('#uiLanguageItem').hidden = true;
  },

  /**
   * Opens the Manage Input Methods page.
   * @private
   */
  onManageInputMethodsTap_: function() {
    settings.navigateTo(settings.routes.INPUT_METHODS);
  },

  /**
   * Handler for tap and <Enter> events on an input method on the main page,
   * which sets it as the current input method.
   * @param {!{model: !{item: !chrome.languageSettingsPrivate.InputMethod},
   *           target: !{tagName: string},
   *           type: string,
   *           key: (string|undefined)}} e
   */
  onInputMethodTap_: function(e) {
    // Taps on the button are handled in onInputMethodOptionsTap_.
    // TODO(dschuyler): The row has two operations that are not clearly
    // delineated. crbug.com/740691
    if (e.target.tagName == 'BUTTON')
      return;

    // Ignore key presses other than <Enter>.
    if (e.type == 'keypress' && e.key != 'Enter')
      return;

    // Set the input method.
    this.languageHelper.setCurrentInputMethod(e.model.item.id);
  },

  /**
   * Opens the input method extension's options page in a new tab (or focuses
   * an existing instance of the IME's options).
   * @param {!{model: !{item: chrome.languageSettingsPrivate.InputMethod}}} e
   * @private
   */
  onInputMethodOptionsTap_: function(e) {
    this.languageHelper.openInputMethodOptions(e.model.item.id);
  },
  // </if>

  // <if expr="chromeos or is_win">
  /**
   * @return {boolean} True for a secondary user in a multi-profile session.
   * @private
   */
  isSecondaryUser_: function() {
    return cr.isChromeOS && loadTimeData.getBoolean('isSecondaryUser');
  },

  /**
   * @param {string} languageCode The language code identifying a language.
   * @param {string} prospectiveUILanguage The prospective UI language.
   * @return {boolean} True if the prospective UI language is set to
   *     |languageCode| but requires a restart to take effect.
   * @private
   */
  isRestartRequired_: function(languageCode, prospectiveUILanguage) {
    return prospectiveUILanguage == languageCode &&
        this.languageHelper.requiresRestart();
  },

  /**
   * @param {!LanguageState} languageState
   * @param {string} prospectiveUILanguage The chosen UI language.
   * @return {boolean} True if the given language cannot be set as the
   *     prospective UI language by the user.
   * @private
   */
  disableUILanguageCheckbox_: function(languageState, prospectiveUILanguage) {
    // UI language setting belongs to the primary user.
    if (this.isSecondaryUser_())
      return true;

    // If the language cannot be a UI language, we can't set it as the
    // prospective UI language.
    if (!languageState.language.supportsUI)
      return true;

    // Unchecking the currently chosen language doesn't make much sense.
    if (languageState.language.code == prospectiveUILanguage)
      return true;

    // Otherwise, the prospective language can be changed to this language.
    return false;
  },

  /**
   * Handler for changes to the UI language checkbox.
   * @param {!{target: !PaperCheckboxElement}} e
   * @private
   */
  onUILanguageChange_: function(e) {
    // We don't support unchecking this checkbox. TODO(michaelpg): Ask for a
    // simpler widget.
    assert(e.target.checked);
    this.languageHelper.setProspectiveUILanguage(
        this.detailLanguage_.language.code);

    this.closeMenuSoon_();
  },
  // </if>

  /**
   * @param {!chrome.languageSettingsPrivate.Language} language
   * @param {string} targetLanguageCode The default translate target language.
   * @return {boolean} True if the translate checkbox should be disabled.
   * @private
   */
  disableTranslateCheckbox_: function(language, targetLanguageCode) {
    if (!language.supportsTranslate)
      return true;

    return this.languageHelper.convertLanguageCodeForTranslate(language.code) ==
        targetLanguageCode;
  },

  /**
   * Handler for changes to the translate checkbox.
   * @param {!{target: !PaperCheckboxElement}} e
   * @private
   */
  onTranslateCheckboxChange_: function(e) {
    if (e.target.checked) {
      this.languageHelper.enableTranslateLanguage(
          this.detailLanguage_.language.code);
    } else {
      this.languageHelper.disableTranslateLanguage(
          this.detailLanguage_.language.code);
    }
    this.closeMenuSoon_();
  },

  /**
   * Returns "complex" if the menu includes checkboxes, which should change the
   * spacing of items and show a separator in the menu.
   * @param {boolean} translateEnabled
   * @return {string}
   */
  getMenuClass_: function(translateEnabled) {
    if (translateEnabled || cr.isChromeOS || cr.isWindows)
      return 'complex';
    return '';
  },

  /**
   * Moves the language to the top of the list.
   * @private
   */
  onMoveToTopTap_: function() {
    /** @type {!CrActionMenuElement} */ (this.$.menu.get()).close();
    this.languageHelper.moveLanguageToFront(this.detailLanguage_.language.code);
  },

  /**
   * Moves the language up in the list.
   * @private
   */
  onMoveUpTap_: function() {
    /** @type {!CrActionMenuElement} */ (this.$.menu.get()).close();
    this.languageHelper.moveLanguage(
        this.detailLanguage_.language.code, true /* upDirection */);
  },

  /**
   * Moves the language down in the list.
   * @private
   */
  onMoveDownTap_: function() {
    /** @type {!CrActionMenuElement} */ (this.$.menu.get()).close();
    this.languageHelper.moveLanguage(
        this.detailLanguage_.language.code, false /* upDirection */);
  },

  /**
   * Disables the language.
   * @private
   */
  onRemoveLanguageTap_: function() {
    /** @type {!CrActionMenuElement} */ (this.$.menu.get()).close();
    this.languageHelper.disableLanguage(this.detailLanguage_.language.code);
  },

  // <if expr="chromeos or is_win">
  /**
   * Checks whether the prospective UI language (the pref that indicates what
   * language to use in Chrome) matches the current language. This pref is used
   * only on Chrome OS and Windows; we don't control the UI language elsewhere.
   * @param {string} languageCode The language code identifying a language.
   * @param {string} prospectiveUILanguage The prospective UI language.
   * @return {boolean} True if the given language matches the prospective UI
   *     pref (which may be different from the actual UI language).
   * @private
   */
  isProspectiveUILanguage_: function(languageCode, prospectiveUILanguage) {
    return languageCode == prospectiveUILanguage;
  },

  /**
   * @param {string} prospectiveUILanguage
   * @return {string}
   * @private
   */
  getProspectiveUILanguageName_: function(prospectiveUILanguage) {
    return this.languageHelper.getLanguage(prospectiveUILanguage).displayName;
  },
  // </if>

  /**
   * @return {string}
   * @private
   */
  getLanguageListTwoLine_: function() {
    return cr.isChromeOS || cr.isWindows ? 'two-line' : '';
  },

  // <if expr="not is_macosx">
  /**
   * Returns the secondary text for the spell check subsection based on the
   * enabled spell check languages, listing at most 2 languages.
   * @return {string}
   * @private
   */
  getSpellCheckSecondaryText_: function() {
    var enabledSpellCheckLanguages =
        this.languages.enabled.filter(function(languageState) {
          return languageState.spellCheckEnabled &&
              languageState.language.supportsSpellcheck;
        });
    switch (enabledSpellCheckLanguages.length) {
      case 0:
        return '';
      case 1:
        return enabledSpellCheckLanguages[0].language.displayName;
      case 2:
        return loadTimeData.getStringF(
            'spellCheckSummaryTwoLanguages',
            enabledSpellCheckLanguages[0].language.displayName,
            enabledSpellCheckLanguages[1].language.displayName);
      case 3:
        // "foo, bar, and 1 other"
        return loadTimeData.getStringF(
            'spellCheckSummaryThreeLanguages',
            enabledSpellCheckLanguages[0].language.displayName,
            enabledSpellCheckLanguages[1].language.displayName);
      default:
        // "foo, bar, and [N-2] others"
        return loadTimeData.getStringF(
            'spellCheckSummaryMultipleLanguages',
            enabledSpellCheckLanguages[0].language.displayName,
            enabledSpellCheckLanguages[1].language.displayName,
            (enabledSpellCheckLanguages.length - 2).toLocaleString());
    }
  },

  /**
   * Opens the Custom Dictionary page.
   * @private
   */
  onEditDictionaryTap_: function() {
    settings.navigateTo(settings.routes.EDIT_DICTIONARY);
  },

  /**
   * Handler for enabling or disabling spell check.
   * @param {!{target: Element, model: !{item: !LanguageState}}} e
   */
  onSpellCheckChange_: function(e) {
    var item = e.model.item;
    if (!item.language.supportsSpellcheck)
      return;

    this.languageHelper.toggleSpellCheck(
        item.language.code, !item.spellCheckEnabled);
  },

  /**
   * @return {string}
   * @private
   */
  getSpellCheckListTwoLine_: function() {
    return this.spellCheckSecondaryText_.length ? 'two-line' : '';
  },
  // </if>

  /**
   * Returns either the "selected" class, if the language matches the
   * prospective UI language, or an empty string. Languages can only be
   * selected on Chrome OS and Windows.
   * @param {string} languageCode The language code identifying a language.
   * @param {string} prospectiveUILanguage The prospective UI language.
   * @return {string} The class name for the language item.
   * @private
   */
  getLanguageItemClass_: function(languageCode, prospectiveUILanguage) {
    if ((cr.isChromeOS || cr.isWindows) &&
        languageCode == prospectiveUILanguage) {
      return 'selected';
    }
    return '';
  },

  // <if expr="chromeos">
  /**
   * @param {string} id The input method ID.
   * @param {string} currentId The ID of the currently enabled input method.
   * @return {boolean} True if the IDs match.
   * @private
   */
  isCurrentInputMethod_: function(id, currentId) {
    assert(cr.isChromeOS);
    return id == currentId;
  },

  /**
   * @param {string} id The input method ID.
   * @param {string} currentId The ID of the currently enabled input method.
   * @return {string} The class for the input method item.
   * @private
   */
  getInputMethodItemClass_: function(id, currentId) {
    assert(cr.isChromeOS);
    return this.isCurrentInputMethod_(id, currentId) ? 'selected' : '';
  },

  getInputMethodName_: function(id) {
    assert(cr.isChromeOS);
    var inputMethod =
        this.languages.inputMethods.enabled.find(function(inputMethod) {
          return inputMethod.id == id;
        });
    return inputMethod ? inputMethod.displayName : '';
  },
  // </if>

  /**
   * @param {!Event} e
   * @private
   */
  onDotsTap_: function(e) {
    // Set a copy of the LanguageState object since it is not data-bound to the
    // languages model directly.
    this.detailLanguage_ = /** @type {!LanguageState} */ (Object.assign(
        {},
        /** @type {!{model: !{item: !LanguageState}}} */ (e).model.item));

    // Ensure the template has been stamped.
    var menu = /** @type {?CrActionMenuElement} */ (this.$.menu.getIfExists());
    if (!menu) {
      menu = /** @type {!CrActionMenuElement} */ (this.$.menu.get());
      // <if expr="chromeos">
      this.tweakMenuForCrOS_(menu);
      // </if>
    }

    menu.showAt(/** @type {!Element} */ (e.target));
  },

  /**
   * @param {boolean} newVal The new value of languagesOpened_.
   * @param {boolean} oldVal The old value of languagesOpened_.
   * @private
   */
  onLanguagesOpenedChanged_: function(newVal, oldVal) {
    if (!oldVal && newVal) {
      chrome.send(
          'metricsHandler:recordBooleanHistogram',
          [LANGUAGE_SETTING_IS_SHOWN_UMA_NAME, true]);
    }
  },

  /**
   * Closes the shared action menu after a short delay, so when a checkbox is
   * tapped it can be seen to change state before disappearing.
   * @private
   */
  closeMenuSoon_: function() {
    var menu = /** @type {!CrActionMenuElement} */ (this.$.menu.get());
    setTimeout(function() {
      if (menu.open)
        menu.close();
    }, settings.kMenuCloseDelay);
  },

  // <if expr="chromeos or is_win">
  /**
   * Handler for the restart button.
   * @private
   */
  onRestartTap_: function() {
    // <if expr="chromeos">
    settings.LifetimeBrowserProxyImpl.getInstance().signOutAndRestart();
    // </if>
    // <if expr="is_win">
    settings.LifetimeBrowserProxyImpl.getInstance().restart();
    // </if>
  },
  // </if>

  /**
   * Toggles the expand button within the element being listened to.
   * @param {!Event} e
   * @private
   */
  toggleExpandButton_: function(e) {
    // The expand button handles toggling itself.
    var expandButtonTag = 'CR-EXPAND-BUTTON';
    if (e.target.tagName == expandButtonTag)
      return;

    /** @type {!CrExpandButtonElement} */
    var expandButton = e.currentTarget.querySelector(expandButtonTag);
    assert(expandButton);
    expandButton.expanded = !expandButton.expanded;
  },
});
})();
