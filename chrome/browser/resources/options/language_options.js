// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(kochi): Generalize the notification as a component and put it
// in js/cr/ui/notification.js .

cr.define('options', function() {

  const OptionsPage = options.OptionsPage;
  const AddLanguageOverlay = options.language.AddLanguageOverlay;
  const LanguageList = options.language.LanguageList;

  // Some input methods like Chinese Pinyin have config pages.
  // This is the map of the input method names to their config page names.
  const INPUT_METHOD_ID_TO_CONFIG_PAGE_NAME = {
    'chewing': 'languageChewing',
    'hangul': 'languageHangul',
    'mozc': 'languageMozc',
    'mozc-dv': 'languageMozc',
    'mozc-jp': 'languageMozc',
    'pinyin': 'languagePinyin',
  };

  /////////////////////////////////////////////////////////////////////////////
  // LanguageOptions class:

  /**
   * Encapsulated handling of ChromeOS language options page.
   * @constructor
   */
  function LanguageOptions(model) {
    OptionsPage.call(this, 'language', localStrings.getString('languagePage'),
                     'languagePage');
  }

  cr.addSingletonGetter(LanguageOptions);

  // Inherit LanguageOptions from OptionsPage.
  LanguageOptions.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * Initializes LanguageOptions page.
     * Calls base class implementation to starts preference initialization.
     */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      var languageOptionsList = $('language-options-list');
      LanguageList.decorate(languageOptionsList);

      languageOptionsList.addEventListener('change',
          this.handleLanguageOptionsListChange_.bind(this));
      languageOptionsList.addEventListener('save',
          this.handleLanguageOptionsListSave_.bind(this));

      this.addEventListener('visibleChange',
                            this.handleVisibleChange_.bind(this));

      if (cr.isChromeOS) {
        this.initializeInputMethodList_();
        this.initializeLanguageCodeToInputMehotdIdsMap_();
      }
      Preferences.getInstance().addEventListener(this.spellCheckDictionaryPref,
          this.handleSpellCheckDictionaryPrefChange_.bind(this));

      // Set up add button.
      $('language-options-add-button').onclick = function(e) {
        // Add the language without showing the overlay if it's specified in
        // the URL hash (ex. lang_add=ja).  Used for automated testing.
        var match = document.location.hash.match(/\blang_add=([\w-]+)/);
        if (match) {
          var addLanguageCode = match[1];
          $('language-options-list').addLanguage(addLanguageCode);
        } else {
          OptionsPage.showOverlay('addLanguageOverlay');
        }
      };
      // Set up remove button.
      $('language-options-remove-button').addEventListener('click',
          this.handleRemoveButtonClick_.bind(this));

      // Setup add language overlay page.
      OptionsPage.registerOverlay(AddLanguageOverlay.getInstance());

      if (cr.isChromeOS) {
        // Listen to user clicks on the add language list.
        var addLanguageList = $('add-language-overlay-language-list');
        addLanguageList.addEventListener('click',
            this.handleAddLanguageListClick_.bind(this));
      } else {
        // Listen to add language dialog ok button.
        var addLanguageOkButton = $('add-language-overlay-ok-button');
        addLanguageOkButton.addEventListener('click',
            this.handleAddLanguageOkButtonClick_.bind(this));

        // Show experimental features if enabled.
        if (templateData.experimentalSpellCheckFeatures == 'true') {
          $('auto-spell-correction-option').classList.remove('hidden');
        }
      }
    },

    // The preference is a CSV string that describes preload engines
    // (i.e. active input methods).
    preloadEnginesPref: 'settings.language.preload_engines',
    // The list of preload engines, like ['mozc', 'pinyin'].
    preloadEngines_: [],
    // The preference is a string that describes the spell check
    // dictionary language, like "en-US".
    spellCheckDictionaryPref: 'spellcheck.dictionary',
    spellCheckDictionary_: "",
    // The map of language code to input method IDs, like:
    // {'ja': ['mozc', 'mozc-jp'], 'zh-CN': ['pinyin'], ...}
    languageCodeToInputMethodIdsMap_: {},

    /**
     * Initializes the input method list.
     */
    initializeInputMethodList_: function() {
      var inputMethodList = $('language-options-input-method-list');
      var inputMethodListData = templateData.inputMethodList;

      // Add all input methods, but make all of them invisible here. We'll
      // change the visibility in handleLanguageOptionsListChange_() based
      // on the selected language. Note that we only have less than 100
      // input methods, so creating DOM nodes at once here should be ok.
      for (var i = 0; i < inputMethodListData.length; i++) {
        var inputMethod = inputMethodListData[i];
        var input = document.createElement('input');
        input.type = 'checkbox';
        input.inputMethodId = inputMethod.id;
        // Listen to user clicks.
        input.addEventListener('click',
                               this.handleCheckboxClick_.bind(this));
        var label = document.createElement('label');
        label.appendChild(input);
        // Adding a space between the checkbox and the text. This is a bit
        // dirty, but we rely on a space character for all other checkboxes.
        label.appendChild(document.createTextNode(
            ' ' + inputMethod.displayName));
        label.style.display = 'none';
        label.languageCodeSet = inputMethod.languageCodeSet;
        // Add the configure button if the config page is present for this
        // input method.
        if (inputMethod.id in INPUT_METHOD_ID_TO_CONFIG_PAGE_NAME) {
          var pageName = INPUT_METHOD_ID_TO_CONFIG_PAGE_NAME[inputMethod.id];
          var button = this.createConfigureInputMethodButton_(inputMethod.id,
                                                              pageName);
          label.appendChild(button);
        }

        inputMethodList.appendChild(label);
      }
      // Listen to pref change once the input method list is initialized.
      Preferences.getInstance().addEventListener(this.preloadEnginesPref,
          this.handlePreloadEnginesPrefChange_.bind(this));
    },

    /**
     * Creates a configure button for the given input method ID.
     * @param {string} inputMethodId Input method ID (ex. "pinyin").
     * @param {string} pageName Name of the config page (ex. "languagePinyin").
     * @private
     */
    createConfigureInputMethodButton_: function(inputMethodId, pageName) {
      var button = document.createElement('button');
      button.textContent = localStrings.getString('configure');
      button.onclick = function(e) {
        // Prevent the default action (i.e. changing the checked property
        // of the checkbox). The button click here should not be handled
        // as checkbox click.
        e.preventDefault();
        chrome.send('inputMethodOptionsOpen', [inputMethodId]);
        OptionsPage.showPageByName(pageName);
      }
      return button;
    },

    /**
     * Handles OptionsPage's visible property change event.
     * @param {Event} e Property change event.
     * @private
     */
    handleVisibleChange_: function(e) {
      if (this.visible) {
        $('language-options-list').redraw();
        chrome.send('languageOptionsOpen');
      }
    },

    /**
     * Handles languageOptionsList's change event.
     * @param {Event} e Change event.
     * @private
     */
    handleLanguageOptionsListChange_: function(e) {
      var languageOptionsList = $('language-options-list');
      var languageCode = languageOptionsList.getSelectedLanguageCode();
      // Select the language if it's specified in the URL hash (ex. lang=ja).
      // Used for automated testing.
      var match = document.location.hash.match(/\blang=([\w-]+)/);
      if (match) {
        var specifiedLanguageCode = match[1];
        if (languageOptionsList.selectLanguageByCode(specifiedLanguageCode)) {
          languageCode = specifiedLanguageCode;
        }
      }
      this.updateSelectedLanguageName_(languageCode);
      if (!cr.isMac)
        this.updateUiLanguageButton_(languageCode);
      this.updateSpellCheckLanguageButton_(languageCode);
      if (cr.isChromeOS)
        this.updateInputMethodList_(languageCode);
      this.updateLanguageListInAddLanguageOverlay_();
    },

    /**
     * Handles languageOptionsList's save event.
     * @param {Event} e Save event.
     * @private
     */
    handleLanguageOptionsListSave_: function(e) {
      if (cr.isChromeOS) {
        // Sort the preload engines per the saved languages before save.
        this.preloadEngines_ = this.sortPreloadEngines_(this.preloadEngines_);
        this.savePreloadEnginesPref_();
      }
    },

    /**
     * Sorts preloadEngines_ by languageOptionsList's order.
     * @param {Array} preloadEngines List of preload engines.
     * @return {Array} Returns sorted preloadEngines.
     * @private
     */
    sortPreloadEngines_: function(preloadEngines) {
      // For instance, suppose we have two languages and associated input
      // methods:
      //
      // - Korean: hangul
      // - Chinese: pinyin
      //
      // The preloadEngines preference should look like "hangul,pinyin".
      // If the user reverse the order, the preference should be reorderd
      // to "pinyin,hangul".
      var languageOptionsList = $('language-options-list');
      var languageCodes = languageOptionsList.getLanguageCodes();

      // Convert the list into a dictonary for simpler lookup.
      var preloadEngineSet = {};
      for (var i = 0; i < preloadEngines.length; i++) {
        preloadEngineSet[preloadEngines[i]] = true;
      }

      // Create the new preload engine list per the language codes.
      var newPreloadEngines = [];
      for (var i = 0; i < languageCodes.length; i++) {
        var languageCode = languageCodes[i];
        var inputMethodIds = this.languageCodeToInputMethodIdsMap_[
            languageCode];
        // Check if we have active input methods associated with the language.
        for (var j = 0; j < inputMethodIds.length; j++) {
          var inputMethodId = inputMethodIds[j];
          if (inputMethodId in preloadEngineSet) {
            // If we have, add it to the new engine list.
            newPreloadEngines.push(inputMethodId);
            // And delete it from the set. This is necessary as one input
            // method can be associated with more than one language thus
            // we should avoid having duplicates in the new list.
            delete preloadEngineSet[inputMethodId];
          }
        }
      }

      return newPreloadEngines;
    },

    /**
     * Initializes the map of language code to input method IDs.
     * @private
     */
    initializeLanguageCodeToInputMehotdIdsMap_: function() {
      var inputMethodList = templateData.inputMethodList;
      for (var i = 0; i < inputMethodList.length; i++) {
        var inputMethod = inputMethodList[i];
        for (var languageCode in inputMethod.languageCodeSet) {
          if (languageCode in this.languageCodeToInputMethodIdsMap_) {
            this.languageCodeToInputMethodIdsMap_[languageCode].push(
                inputMethod.id);
          } else {
            this.languageCodeToInputMethodIdsMap_[languageCode] =
                [inputMethod.id];
          }
        }
      }
    },

    /**
     * Updates the currently selected language name.
     * @param {string} languageCode Language code (ex. "fr").
     * @private
     */
    updateSelectedLanguageName_: function(languageCode) {
      var languageDisplayName = LanguageList.getDisplayNameFromLanguageCode(
          languageCode);
      var languageNativeDisplayName =
          LanguageList.getNativeDisplayNameFromLanguageCode(languageCode);
      // If the native name is different, add it.
      if (languageDisplayName != languageNativeDisplayName) {
        languageDisplayName += ' - ' + languageNativeDisplayName;
      }
      // Update the currently selected language name.
      $('language-options-language-name').textContent = languageDisplayName;
    },

    /**
     * Updates the UI language button.
     * @param {string} languageCode Language code (ex. "fr").
     * @private
     */
    updateUiLanguageButton_: function(languageCode) {
      var uiLanguageButton = $('language-options-ui-language-button');
      // Check if the language code matches the current UI language.
      if (languageCode == templateData.currentUiLanguageCode) {
        // If it matches, the button just says that the UI language is
        // currently in use.
        uiLanguageButton.textContent =
            localStrings.getString('is_displayed_in_this_language');
        // Make it look like a text label.
        uiLanguageButton.className = 'text-button';
        // Remove the event listner.
        uiLanguageButton.onclick = undefined;
      } else if (languageCode in templateData.uiLanguageCodeSet) {
        // If the language is supported as UI language, users can click on
        // the button to change the UI language.
        uiLanguageButton.textContent =
            localStrings.getString('display_in_this_language');
        uiLanguageButton.className = '';
        // Send the change request to Chrome.
        uiLanguageButton.onclick = function(e) {
          chrome.send('uiLanguageChange', [languageCode]);
        }
        $('language-options-ui-restart-button').onclick = function(e) {
          chrome.send('uiLanguageRestart');
        }
      } else {
        // If the language is not supported as UI language, the button
        // just says that Chromium OS cannot be displayed in this language.
        uiLanguageButton.textContent =
            localStrings.getString('cannot_be_displayed_in_this_language');
        uiLanguageButton.className = 'text-button';
        uiLanguageButton.onclick = undefined;
      }
      uiLanguageButton.style.display = 'block';
      $('language-options-ui-notification-bar').style.display = 'none';
    },

    /**
     * Updates the spell check language button.
     * @param {string} languageCode Language code (ex. "fr").
     * @private
     */
    updateSpellCheckLanguageButton_: function(languageCode) {
      var spellCheckLanguageButton = $(
          'language-options-spell-check-language-button');
      // Check if the language code matches the current spell check language.
      if (languageCode == this.spellCheckDictionary_) {
        // If it matches, the button just says that the spell check language is
        // currently in use.
        spellCheckLanguageButton.textContent =
            localStrings.getString('is_used_for_spell_checking');
        // Make it look like a text label.
        spellCheckLanguageButton.className = 'text-button';
        // Remove the event listner.
        spellCheckLanguageButton.onclick = undefined;
      } else if (languageCode in templateData.spellCheckLanguageCodeSet) {
        // If the language is supported as spell check language, users can
        // click on the button to change the spell check language.
        spellCheckLanguageButton.textContent =
            localStrings.getString('use_this_for_spell_checking');
        spellCheckLanguageButton.className = '';
        spellCheckLanguageButton.languageCode = languageCode;
        // Add an event listner to the click event.
        spellCheckLanguageButton.addEventListener('click',
            this.handleSpellCheckLanguageButtonClick_.bind(this));
      } else {
        // If the language is not supported as spell check language, the
        // button just says that this language cannot be used for spell
        // checking.
        spellCheckLanguageButton.textContent =
            localStrings.getString('cannot_be_used_for_spell_checking');
        spellCheckLanguageButton.className = 'text-button';
        spellCheckLanguageButton.onclick = undefined;
      }
      spellCheckLanguageButton.style.display = 'block';
      $('language-options-ui-notification-bar').style.display = 'none';
    },

    /**
     * Updates the input method list.
     * @param {string} languageCode Language code (ex. "fr").
     * @private
     */
    updateInputMethodList_: function(languageCode) {
      // Give one of the checkboxes or buttons focus, if it's specified in the
      // URL hash (ex. focus=mozc). Used for automated testing.
      var focusInputMethodId = -1;
      var match = document.location.hash.match(/\bfocus=([\w:-]+)\b/);
      if (match) {
        focusInputMethodId = match[1];
      }
      // Change the visibility of the input method list. Input methods that
      // matches |languageCode| will become visible.
      var inputMethodList = $('language-options-input-method-list');
      var labels = inputMethodList.querySelectorAll('label');
      for (var i = 0; i < labels.length; i++) {
        var label = labels[i];
        if (languageCode in label.languageCodeSet) {
          label.style.display = 'block';
          var input = label.childNodes[0];
          // Give it focus if the ID matches.
          if (input.inputMethodId == focusInputMethodId) {
            input.focus();
          }
        } else {
          label.style.display = 'none';
        }
      }
      if (focusInputMethodId == 'remove') {
        $('language-options-remove-button').focus();
      } else if (focusInputMethodId == 'add') {
        $('language-options-add-button').focus();
      }
    },

    /**
     * Updates the language list in the add language overlay.
     * @param {string} languageCode Language code (ex. "fr").
     * @private
     */
    updateLanguageListInAddLanguageOverlay_: function(languageCode) {
      // Change the visibility of the language list in the add language
      // overlay. Languages that are already active will become invisible,
      // so that users don't add the same language twice.
      var languageOptionsList = $('language-options-list');
      var languageCodes = languageOptionsList.getLanguageCodes();
      var languageCodeSet = {};
      for (var i = 0; i < languageCodes.length; i++) {
        languageCodeSet[languageCodes[i]] = true;
      }
      var addLanguageList = $('add-language-overlay-language-list');
      var lis = addLanguageList.querySelectorAll('li');
      for (var i = 0; i < lis.length; i++) {
        // The first child button knows the language code.
        var button = lis[i].childNodes[0];
        if (button.languageCode in languageCodeSet) {
          lis[i].style.display = 'none';
        } else {
          lis[i].style.display = 'block';
        }
      }
    },

    /**
     * Handles preloadEnginesPref change.
     * @param {Event} e Change event.
     * @private
     */
    handlePreloadEnginesPrefChange_: function(e) {
      var value = e.value.value;
      this.preloadEngines_ = this.filterBadPreloadEngines_(value.split(','));
      this.updateCheckboxesFromPreloadEngines_();
    },

    /**
     * Handles input method checkbox's click event.
     * @param {Event} e Click event.
     * @private
     */
    handleCheckboxClick_ : function(e) {
      var checkbox = e.target;
      if (this.preloadEngines_.length == 1 && !checkbox.checked) {
        // Don't allow disabling the last input method.
        this.showNotification_(
            localStrings.getString('please_add_another_input_method'),
            localStrings.getString('ok_button'));
        checkbox.checked = true;
        return;
      }
      if (checkbox.checked) {
        chrome.send('inputMethodEnable', [checkbox.inputMethodId]);
      } else {
        chrome.send('inputMethodDisable', [checkbox.inputMethodId]);
      }
      this.updatePreloadEnginesFromCheckboxes_();
      this.preloadEngines_ = this.sortPreloadEngines_(this.preloadEngines_);
      this.savePreloadEnginesPref_();
    },

    /**
     * Handles add language list's click event.
     * @param {Event} e Click event.
     */
    handleAddLanguageListClick_ : function(e) {
      var languageOptionsList = $('language-options-list');
      var languageCode = e.target.languageCode;
      // languageCode can be undefined, if click was made on some random
      // place in the overlay, rather than a button. Ignore it.
      if (!languageCode) {
        return;
      }
      languageOptionsList.addLanguage(languageCode);
      var inputMethodIds = this.languageCodeToInputMethodIdsMap_[languageCode];
      // Enable the first input method for the language added.
      if (inputMethodIds && inputMethodIds[0] &&
          // Don't add the input method it's already present. This can
          // happen if the same input method is shared among multiple
          // languages (ex. English US keyboard is used for English US and
          // Filipino).
          this.preloadEngines_.indexOf(inputMethodIds[0]) == -1) {
        this.preloadEngines_.push(inputMethodIds[0]);
        this.updateCheckboxesFromPreloadEngines_();
        this.savePreloadEnginesPref_();
      }
      OptionsPage.clearOverlays();
    },

    /**
     * Handles add language dialog ok button.
     */
    handleAddLanguageOkButtonClick_ : function() {
      var languagesSelect = $('add-language-overlay-language-list');
      var selectedIndex = languagesSelect.selectedIndex;
      if (selectedIndex >= 0) {
        var selection = languagesSelect.options[selectedIndex];
        $('language-options-list').addLanguage(String(selection.value));
        OptionsPage.clearOverlays();
      }
    },

    /**
     * Handles remove button's click event.
     * @param {Event} e Click event.
     */
    handleRemoveButtonClick_: function(e) {
      var languageOptionsList = $('language-options-list');
      var languageCode = languageOptionsList.getSelectedLanguageCode();
      // Don't allow removing the language if it's as UI language.
      if (languageCode == templateData.currentUiLanguageCode) {
        this.showNotification_(
            localStrings.getString('this_language_is_currently_in_use'),
            localStrings.getString('ok_button'));
        return;
      }
      if (cr.isChromeOS) {
        // Disable input methods associated with |languageCode|.
        // Don't allow removing the language if cerntain conditions are met.
        // See removePreloadEnginesByLanguageCode_() for details.
        if (!this.removePreloadEnginesByLanguageCode_(languageCode)) {
          this.showNotification_(
              localStrings.getString('please_add_another_language'),
              localStrings.getString('ok_button'));
          return;
        }
      }
      languageOptionsList.removeSelectedLanguage();
    },

    /**
     * Handles spellCheckDictionaryPref change.
     * @param {Event} e Change event.
     * @private
     */
    handleSpellCheckDictionaryPrefChange_: function(e) {
      var languageCode = e.value.value
      this.spellCheckDictionary_ = languageCode;
      var languageOptionsList = $('language-options-list');
      var selectedLanguageCode = languageOptionsList.getSelectedLanguageCode();
      this.updateSpellCheckLanguageButton_(selectedLanguageCode);
    },

    /**
     * Handles spellCheckLanguageButton click.
     * @param {Event} e Click event.
     * @private
     */
    handleSpellCheckLanguageButtonClick_: function(e) {
      var languageCode = e.target.languageCode;
      // Save the preference.
      Preferences.setStringPref(this.spellCheckDictionaryPref,
                                languageCode);
      chrome.send('spellCheckLanguageChange', [languageCode]);
    },

    /**
     * Removes preload engines associated with the given language code.
     * However, this function does not remove engines (input methods) that
     * are used for other active languages. For instance, if "xkb:us::eng"
     * is used for English and Filipino, and the two languages are active,
     * this function does not remove "xkb:us::eng" when either of these
     * languages is removed. Instead, it'll remove "xkb:us::eng" when the
     * both languages are gone.
     *
     * @param {string} languageCode Language code (ex. "fr").
     * @return {boolean} Returns true on success.
     * @private
     */
    removePreloadEnginesByLanguageCode_: function(languageCode) {
      // First create the set of engines to be removed from input methods
      // associated with the language code.
      var enginesToBeRemovedSet = {};
      var inputMethodIds = this.languageCodeToInputMethodIdsMap_[languageCode];
      for (var i = 0; i < inputMethodIds.length; i++) {
        enginesToBeRemovedSet[inputMethodIds[i]] = true;
      }

      // Then eliminate engines that are also used for other active languages.
      var languageCodes = $('language-options-list').getLanguageCodes();
      for (var i = 0; i < languageCodes.length; i++) {
        // Skip the target language code.
        if (languageCodes[i] == languageCode) {
          continue;
        }
        // Check if input methods used in this language are included in
        // enginesToBeRemovedSet. If so, eliminate these from the set, so
        // we don't remove this time.
        var inputMethodIdsForAnotherLanguage =
            this.languageCodeToInputMethodIdsMap_[languageCodes[i]];
        for (var j = 0; j < inputMethodIdsForAnotherLanguage.length; j++) {
          var inputMethodId = inputMethodIdsForAnotherLanguage[j];
          if (inputMethodId in enginesToBeRemovedSet) {
            delete enginesToBeRemovedSet[inputMethodId];
          }
        }
      }

      // Update the preload engine list with the to-be-removed set.
      var newPreloadEngines = [];
      for (var i = 0; i < this.preloadEngines_.length; i++) {
        if (!(this.preloadEngines_[i] in enginesToBeRemovedSet)) {
          newPreloadEngines.push(this.preloadEngines_[i]);
        }
      }
      // Don't allow this operation if it causes the number of preload
      // engines to be zero.
      if (newPreloadEngines.length == 0) {
        return false;
      }
      this.preloadEngines_ = newPreloadEngines;
      this.savePreloadEnginesPref_();
      return true;
    },

    /**
     * Saves the preload engines preference.
     * @private
     */
    savePreloadEnginesPref_: function() {
      Preferences.setStringPref(this.preloadEnginesPref,
                                this.preloadEngines_.join(','));
    },

    /**
     * Updates the checkboxes in the input method list from the preload
     * engines preference.
     * @private
     */
    updateCheckboxesFromPreloadEngines_: function() {
      // Convert the list into a dictonary for simpler lookup.
      var dictionary = {};
      for (var i = 0; i < this.preloadEngines_.length; i++) {
        dictionary[this.preloadEngines_[i]] = true;
      }

      var inputMethodList = $('language-options-input-method-list');
      var checkboxes = inputMethodList.querySelectorAll('input');
      for (var i = 0; i < checkboxes.length; i++) {
        checkboxes[i].checked = (checkboxes[i].inputMethodId in dictionary);
      }
    },

    /**
     * Updates the preload engines preference from the checkboxes in the
     * input method list.
     * @private
     */
    updatePreloadEnginesFromCheckboxes_: function() {
      this.preloadEngines_ = [];
      var inputMethodList = $('language-options-input-method-list');
      var checkboxes = inputMethodList.querySelectorAll('input');
      for (var i = 0; i < checkboxes.length; i++) {
        if (checkboxes[i].checked) {
          this.preloadEngines_.push(checkboxes[i].inputMethodId);
        }
      }
    },

    /**
     * Filters bad preload engines in case bad preload engines are
     * stored in the preference. Removes duplicates as well.
     * @param {Array} preloadEngines List of preload engines.
     * @private
     */
    filterBadPreloadEngines_: function(preloadEngines) {
      // Convert the list into a dictonary for simpler lookup.
      var dictionary = {};
      for (var i = 0; i < templateData.inputMethodList.length; i++) {
        dictionary[templateData.inputMethodList[i].id] = true;
      }

      var filteredPreloadEngines = [];
      var seen = {};
      for (var i = 0; i < preloadEngines.length; i++) {
        // Check if the preload engine is present in the
        // dictionary, and not duplicate. Otherwise, skip it.
        if (preloadEngines[i] in dictionary && !(preloadEngines[i] in seen)) {
          filteredPreloadEngines.push(preloadEngines[i]);
          seen[preloadEngines[i]] = true;
        }
      }
      return filteredPreloadEngines;
    },

    // TODO(kochi): This is an adapted copy from new_new_tab.js.
    // If this will go as final UI, refactor this to share the component with
    // new new tab page.
    /**
     * Shows notification
     * @private
     */
    notificationTimeout_: null,
    showNotification_ : function(text, actionText, opt_delay) {
      var notificationElement = $('notification');
      var actionLink = notificationElement.querySelector('.link-color');
      var delay = opt_delay || 10000;

      function show() {
        window.clearTimeout(this.notificationTimeout_);
        notificationElement.classList.add('show');
        document.body.classList.add('notification-shown');
      }

      function hide() {
        window.clearTimeout(this.notificationTimeout_);
        notificationElement.classList.remove('show');
        document.body.classList.remove('notification-shown');
        // Prevent tabbing to the hidden link.
        actionLink.tabIndex = -1;
        // Setting tabIndex to -1 only prevents future tabbing to it. If,
        // however, the user switches window or a tab and then moves back to
        // this tab the element may gain focus. We therefore make sure that we
        // blur the element so that the element focus is not restored when
        // coming back to this window.
        actionLink.blur();
      }

      function delayedHide() {
        this.notificationTimeout_ = window.setTimeout(hide, delay);
      }

      notificationElement.firstElementChild.textContent = text;
      actionLink.textContent = actionText;

      actionLink.onclick = hide;
      actionLink.onkeydown = function(e) {
        if (e.keyIdentifier == 'Enter') {
          hide();
        }
      };
      notificationElement.onmouseover = show;
      notificationElement.onmouseout = delayedHide;
      actionLink.onfocus = show;
      actionLink.onblur = delayedHide;
      // Enable tabbing to the link now that it is shown.
      actionLink.tabIndex = 0;

      show();
      delayedHide();
    }
  };

  /**
   * Chrome callback for when the UI language preference is saved.
   */
  LanguageOptions.uiLanguageSaved = function() {
    $('language-options-ui-language-button').style.display = 'none';
    $('language-options-ui-notification-bar').style.display = 'block';
  };

  // Export
  return {
    LanguageOptions: LanguageOptions
  };

});
