// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(kochi): Generalize the notification as a component and put it
// in js/cr/ui/notification.js .

cr.define('options', function() {
  /** @const */ var OptionsPage = options.OptionsPage;
  /** @const */ var LanguageList = options.LanguageList;

  // Some input methods like Chinese Pinyin have config pages.
  // This is the map of the input method names to their config page names.
  /** @const */ var INPUT_METHOD_ID_TO_CONFIG_PAGE_NAME = {
    'mozc': 'languageMozc',
    'mozc-chewing': 'languageChewing',
    'mozc-dv': 'languageMozc',
    'mozc-hangul': 'languageHangul',
    'mozc-jp': 'languageMozc',
    'pinyin': 'languagePinyin',
    'pinyin-dv': 'languagePinyin',
  };

  /**
   * Spell check dictionary download status.
   * @type {Enum}
   */
  /** @const*/ var DOWNLOAD_STATUS = {
    IN_PROGRESS: 1,
    FAILED: 2
  };

  /////////////////////////////////////////////////////////////////////////////
  // LanguageOptions class:

  /**
   * Encapsulated handling of ChromeOS language options page.
   * @constructor
   */
  function LanguageOptions(model) {
    OptionsPage.call(this, 'languages',
                     loadTimeData.getString('languagePageTabTitle'),
                     'languagePage');
  }

  cr.addSingletonGetter(LanguageOptions);

  // Inherit LanguageOptions from OptionsPage.
  LanguageOptions.prototype = {
    __proto__: OptionsPage.prototype,

    /* For recording the prospective language (the next locale after relaunch).
     * @type {?string}
     * @private
     */
    prospectiveUiLanguageCode_: null,

    /*
     * Map from language code to spell check dictionary download status for that
     * language.
     * @type {Array}
     * @private
     */
    spellcheckDictionaryDownloadStatus_: [],

    /**
     * Number of times a spell check dictionary download failed.
     * @type {int}
     * @private
     */
    spellcheckDictionaryDownloadFailures_: 0,

    /**
     * Initializes LanguageOptions page.
     * Calls base class implementation to start preference initialization.
     */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      var languageOptionsList = $('language-options-list');
      LanguageList.decorate(languageOptionsList);

      languageOptionsList.addEventListener('change',
          this.handleLanguageOptionsListChange_.bind(this));
      languageOptionsList.addEventListener('save',
          this.handleLanguageOptionsListSave_.bind(this));

      this.prospectiveUiLanguageCode_ =
          loadTimeData.getString('prospectiveUiLanguageCode');
      this.addEventListener('visibleChange',
                            this.handleVisibleChange_.bind(this));

      if (cr.isChromeOS) {
        $('chewing-confirm').onclick = $('hangul-confirm').onclick =
            $('mozc-confirm').onclick = $('pinyin-confirm').onclick =
                OptionsPage.closeOverlay.bind(OptionsPage);

        this.initializeInputMethodList_();
        this.initializeLanguageCodeToInputMethodIdsMap_();
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
          OptionsPage.navigateToPage('addLanguage');
        }
      };

      if (!cr.isMac) {
        // Set up the button for editing custom spelling dictionary.
        $('edit-dictionary-button').onclick = function(e) {
          OptionsPage.navigateToPage('editDictionary');
        };
        $('dictionary-download-retry-button').onclick = function(e) {
          chrome.send('retryDictionaryDownload');
        };
      }

      if (cr.isChromeOS) {
        // Listen to user clicks on the add language list.
        var addLanguageList = $('add-language-overlay-language-list');
        addLanguageList.addEventListener(
            'click',
            this.handleAddLanguageListClick_.bind(this));
        $('language-options-extension-ime-button').addEventListener(
            'click',
            this.handleExtensionImeButtonClick_.bind(this));
      } else {
        // Listen to add language dialog ok button.
        var addLanguageOkButton = $('add-language-overlay-ok-button');
        addLanguageOkButton.addEventListener(
            'click',
            this.handleAddLanguageOkButtonClick_.bind(this));

        // Show experimental features if enabled.
        if (loadTimeData.getBoolean('enableSpellingAutoCorrect'))
          $('auto-spell-correction-option').hidden = false;

        // Handle spell check enable/disable.
        if (!cr.isMac) {
          Preferences.getInstance().addEventListener(
              this.enableSpellCheckPref,
              this.updateEnableSpellCheck_.bind(this));
        }
      }

      // Handle clicks on "Use this language for spell checking" button.
      if (!cr.isMac) {
        var spellCheckLanguageButton = getRequiredElement(
            'language-options-spell-check-language-button');
        spellCheckLanguageButton.addEventListener(
            'click',
            this.handleSpellCheckLanguageButtonClick_.bind(this));
      }

      if (cr.isChromeOS) {
        $('language-options-ui-restart-button').onclick = function() {
          chrome.send('uiLanguageRestart');
        };
      }

      $('language-confirm').onclick =
          OptionsPage.closeOverlay.bind(OptionsPage);
    },

    // The preference is a boolean that enables/disables spell checking.
    enableSpellCheckPref: 'browser.enable_spellchecking',
    // The preference is a CSV string that describes preload engines
    // (i.e. active input methods).
    preloadEnginesPref: 'settings.language.preload_engines',
    // The list of preload engines, like ['mozc', 'pinyin'].
    preloadEngines_: [],
    // The preference that lists the extension IMEs that are filtered out of
    // the language menu.
    filteredExtensionImesPref: 'settings.language.filtered_extension_imes',
    // The list of extension IMEs that are filtered out of the language menu.
    filteredExtensionImes_: [],
    // The preference is a string that describes the spell check
    // dictionary language, like "en-US".
    spellCheckDictionaryPref: 'spellcheck.dictionary',
    spellCheckDictionary_: '',
    // The map of language code to input method IDs, like:
    // {'ja': ['mozc', 'mozc-jp'], 'zh-CN': ['pinyin'], ...}
    languageCodeToInputMethodIdsMap_: {},

    /**
     * Initializes the input method list.
     */
    initializeInputMethodList_: function() {
      var inputMethodList = $('language-options-input-method-list');
      var inputMethodListData = loadTimeData.getValue('inputMethodList');
      var inputMethodPrototype = $('language-options-input-method-proto');

      // Add all input methods, but make all of them invisible here. We'll
      // change the visibility in handleLanguageOptionsListChange_() based
      // on the selected language. Note that we only have less than 100
      // input methods, so creating DOM nodes at once here should be ok.
      for (var i = 0; i < inputMethodListData.length; i++) {
        var inputMethod = inputMethodListData[i];
        var element = inputMethodPrototype.cloneNode(true);
        element.id = '';
        element.languageCodeSet = inputMethod.languageCodeSet;
        var input = element.querySelectorAll('input')[0];
        input.inputMethodId = inputMethod.id;
        var span = element.querySelectorAll('span')[0];
        span.textContent = inputMethod.displayName;

        // Listen to user clicks.
        input.addEventListener('click',
                               this.handleCheckboxClick_.bind(this));

        // Add the configure button if the config page is present for this
        // input method.
        if (inputMethod.id in INPUT_METHOD_ID_TO_CONFIG_PAGE_NAME) {
          var pageName = INPUT_METHOD_ID_TO_CONFIG_PAGE_NAME[inputMethod.id];
          var button = this.createConfigureInputMethodButton_(inputMethod.id,
                                                              pageName);
          element.appendChild(button);
        }
        inputMethodList.appendChild(element);
      }

      var extensionImeList = loadTimeData.getValue('extensionImeList');
      for (var i = 0; i < extensionImeList.length; i++) {
        var inputMethod = extensionImeList[i];
        var element = inputMethodPrototype.cloneNode(true);
        element.id = '';
        element.languageCodeSet = {};
        var input = element.querySelectorAll('input')[0];
        input.inputMethodId = inputMethod.id;
        var span = element.querySelectorAll('span')[0];
        span.textContent = inputMethod.displayName;

        input.addEventListener('click',
                               this.handleExtensionCheckboxClick_.bind(this));

        inputMethodList.appendChild(element);
      }

      // Listen to pref change once the input method list is initialized.
      Preferences.getInstance().addEventListener(
          this.preloadEnginesPref,
          this.handlePreloadEnginesPrefChange_.bind(this));
      Preferences.getInstance().addEventListener(
          this.filteredExtensionImesPref,
          this.handleFilteredExtensionsPrefChange_.bind(this));
    },

    /**
     * Creates a configure button for the given input method ID.
     * @param {string} inputMethodId Input method ID (ex. "pinyin").
     * @param {string} pageName Name of the config page (ex. "languagePinyin").
     * @private
     */
    createConfigureInputMethodButton_: function(inputMethodId, pageName) {
      var button = document.createElement('button');
      button.textContent = loadTimeData.getString('configure');
      button.onclick = function(e) {
        // Prevent the default action (i.e. changing the checked property
        // of the checkbox). The button click here should not be handled
        // as checkbox click.
        e.preventDefault();
        chrome.send('inputMethodOptionsOpen', [inputMethodId]);
        OptionsPage.navigateToPage(pageName);
      };
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

      // If there's no selection, just return.
      if (!languageCode)
        return;

      // Select the language if it's specified in the URL hash (ex. lang=ja).
      // Used for automated testing.
      var match = document.location.hash.match(/\blang=([\w-]+)/);
      if (match) {
        var specifiedLanguageCode = match[1];
        if (languageOptionsList.selectLanguageByCode(specifiedLanguageCode)) {
          languageCode = specifiedLanguageCode;
        }
      }

      if (cr.isWindows || cr.isChromeOS)
        this.updateUiLanguageButton_(languageCode);

      if (!cr.isMac) {
        this.updateSelectedLanguageName_(languageCode);
        this.updateSpellCheckLanguageButton_(languageCode);
      }

      if (cr.isChromeOS)
        this.updateInputMethodList_(languageCode);

      this.updateLanguageListInAddLanguageOverlay_();
    },

    /**
     * Happens when a user changes back to the language they're currently using.
     */
    currentLocaleWasReselected: function() {
      this.updateUiLanguageButton_(
          loadTimeData.getString('currentUiLanguageCode'));
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
    initializeLanguageCodeToInputMethodIdsMap_: function() {
      var inputMethodList = loadTimeData.getValue('inputMethodList');
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
      var languageInfo = LanguageList.getLanguageInfoFromLanguageCode(
          languageCode);
      var languageDisplayName = languageInfo.displayName;
      var languageNativeDisplayName = languageInfo.nativeDisplayName;
      var textDirection = languageInfo.textDirection;

      // If the native name is different, add it.
      if (languageDisplayName != languageNativeDisplayName) {
        languageDisplayName += ' - ' + languageNativeDisplayName;
      }

      // Update the currently selected language name.
      var languageName = $('language-options-language-name');
      languageName.textContent = languageDisplayName;
      languageName.dir = textDirection;
    },

    /**
     * Updates the UI language button.
     * @param {string} languageCode Language code (ex. "fr").
     * @private
     */
    updateUiLanguageButton_: function(languageCode) {
      var uiLanguageButton = $('language-options-ui-language-button');
      var uiLanguageMessage = $('language-options-ui-language-message');
      var uiLanguageNotification = $('language-options-ui-notification-bar');

      // Remove the event listener and add it back if useful.
      uiLanguageButton.onclick = null;

      // Unhide the language button every time, as it could've been previously
      // hidden by a language change.
      uiLanguageButton.hidden = false;

      if (languageCode == this.prospectiveUiLanguageCode_) {
        uiLanguageMessage.textContent =
            loadTimeData.getString('is_displayed_in_this_language');
        showMutuallyExclusiveNodes(
            [uiLanguageButton, uiLanguageMessage, uiLanguageNotification], 1);
      } else if (languageCode in loadTimeData.getValue('uiLanguageCodeSet')) {
        if (cr.isChromeOS && UIAccountTweaks.loggedInAsGuest()) {
          // In the guest mode for ChromeOS, changing UI language does not make
          // sense because it does not take effect after browser restart.
          uiLanguageButton.hidden = true;
          uiLanguageMessage.hidden = true;
        } else {
          uiLanguageButton.textContent =
              loadTimeData.getString('display_in_this_language');
          showMutuallyExclusiveNodes(
              [uiLanguageButton, uiLanguageMessage, uiLanguageNotification], 0);
          uiLanguageButton.onclick = function(e) {
            chrome.send('uiLanguageChange', [languageCode]);
          };
        }
      } else {
        uiLanguageMessage.textContent =
            loadTimeData.getString('cannot_be_displayed_in_this_language');
        showMutuallyExclusiveNodes(
            [uiLanguageButton, uiLanguageMessage, uiLanguageNotification], 1);
      }
    },

    /**
     * Updates the spell check language button.
     * @param {string} languageCode Language code (ex. "fr").
     * @private
     */
    updateSpellCheckLanguageButton_: function(languageCode) {
      var spellCheckLanguageSection = $('language-options-spellcheck');
      var spellCheckLanguageButton =
          $('language-options-spell-check-language-button');
      var spellCheckLanguageMessage =
          $('language-options-spell-check-language-message');
      var dictionaryDownloadInProgress =
          $('language-options-dictionary-downloading-message');
      var dictionaryDownloadFailed =
          $('language-options-dictionary-download-failed-message');
      var dictionaryDownloadFailHelp =
          $('language-options-dictionary-download-fail-help-message');
      spellCheckLanguageSection.hidden = false;
      spellCheckLanguageMessage.hidden = true;
      spellCheckLanguageButton.hidden = true;
      dictionaryDownloadInProgress.hidden = true;
      dictionaryDownloadFailed.hidden = true;
      dictionaryDownloadFailHelp.hidden = true;

      if (languageCode == this.spellCheckDictionary_) {
        if (!(languageCode in this.spellcheckDictionaryDownloadStatus_)) {
          spellCheckLanguageMessage.textContent =
              loadTimeData.getString('is_used_for_spell_checking');
          showMutuallyExclusiveNodes(
              [spellCheckLanguageButton, spellCheckLanguageMessage], 1);
        } else if (this.spellcheckDictionaryDownloadStatus_[languageCode] ==
                       DOWNLOAD_STATUS.IN_PROGRESS) {
          dictionaryDownloadInProgress.hidden = false;
        } else if (this.spellcheckDictionaryDownloadStatus_[languageCode] ==
                       DOWNLOAD_STATUS.FAILED) {
          spellCheckLanguageSection.hidden = true;
          dictionaryDownloadFailed.hidden = false;
          if (this.spellcheckDictionaryDownloadFailures_ > 1)
            dictionaryDownloadFailHelp.hidden = false;
        }
      } else if (languageCode in
          loadTimeData.getValue('spellCheckLanguageCodeSet')) {
        spellCheckLanguageButton.textContent =
            loadTimeData.getString('use_this_for_spell_checking');
        showMutuallyExclusiveNodes(
            [spellCheckLanguageButton, spellCheckLanguageMessage], 0);
        spellCheckLanguageButton.languageCode = languageCode;
      } else if (!languageCode) {
        spellCheckLanguageButton.hidden = true;
        spellCheckLanguageMessage.hidden = true;
      } else {
        spellCheckLanguageMessage.textContent =
            loadTimeData.getString('cannot_be_used_for_spell_checking');
        showMutuallyExclusiveNodes(
            [spellCheckLanguageButton, spellCheckLanguageMessage], 1);
      }
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
      var methods = inputMethodList.querySelectorAll('.input-method');
      for (var i = 0; i < methods.length; i++) {
        var method = methods[i];
        if (languageCode in method.languageCodeSet) {
          method.hidden = false;
          var input = method.querySelectorAll('input')[0];
          // Give it focus if the ID matches.
          if (input.inputMethodId == focusInputMethodId) {
            input.focus();
          }
        } else {
          method.hidden = true;
        }
      }

      if (focusInputMethodId == 'add') {
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
      $('language-options-list').updateDeletable();
    },

    /**
     * Handles filteredExtensionImesPref change.
     * @param {Event} e Change event.
     * @private
     */
    handleFilteredExtensionsPrefChange_: function(e) {
      var value = e.value.value;
      this.filteredExtensionImes_ = value.split(',');
      this.updateCheckboxesFromFilteredExtensions_();
    },

    /**
     * Handles input method checkbox's click event.
     * @param {Event} e Click event.
     * @private
     */
    handleCheckboxClick_: function(e) {
      var checkbox = e.target;
      if (this.preloadEngines_.length == 1 && !checkbox.checked) {
        // Don't allow disabling the last input method.
        this.showNotification_(
            loadTimeData.getString('please_add_another_input_method'),
            loadTimeData.getString('ok_button'));
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
     * Handles extension input method checkbox's click event.
     * @param {Event} e Click event.
     * @private
     */
    handleExtensionCheckboxClick_: function(e) {
      var checkbox = e.target;
      this.updateFilteredExtensionsFromCheckboxes_();
      this.saveFilteredExtensionPref_();
    },

    /**
     * Handles add language list's click event.
     * @param {Event} e Click event.
     */
    handleAddLanguageListClick_: function(e) {
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
      OptionsPage.closeOverlay();
    },

    /**
     * Handles extension IME button.
     */
    handleExtensionImeButtonClick_: function() {
      $('language-options-list').clearSelection();

      var languageName = $('language-options-language-name');
      languageName.textContent = loadTimeData.getString('extension_ime_label');

      var uiLanguageMessage = $('language-options-ui-language-message');
      uiLanguageMessage.textContent =
          loadTimeData.getString('extension_ime_description');

      var uiLanguageButton = $('language-options-ui-language-button');
      uiLanguageButton.onclick = null;
      uiLanguageButton.hidden = true;

      this.updateSpellCheckLanguageButton_();

      // Hide all input method checkboxes that aren't extension IMEs.
      var inputMethodList = $('language-options-input-method-list');
      var methods = inputMethodList.querySelectorAll('.input-method');
      for (var i = 0; i < methods.length; i++) {
        var method = methods[i];
        var input = method.querySelectorAll('input')[0];
        // Give it focus if the ID matches.
        if (input.inputMethodId.match(/^_ext_ime_/))
          method.hidden = false;
        else
          method.hidden = true;
      }
    },

    handleAddLanguageOkButtonClick_: function() {
      var languagesSelect = $('add-language-overlay-language-list');
      var selectedIndex = languagesSelect.selectedIndex;
      if (selectedIndex >= 0) {
        var selection = languagesSelect.options[selectedIndex];
        $('language-options-list').addLanguage(String(selection.value));
        OptionsPage.closeOverlay();
      }
    },

    /**
     * Checks if languageCode is deletable or not.
     * @param {string} languageCode the languageCode to check for deletability.
     */
    languageIsDeletable: function(languageCode) {
      // Don't allow removing the language if it's a UI language.
      if (languageCode == this.prospectiveUiLanguageCode_)
        return false;
      return (!cr.isChromeOS ||
              this.canDeleteLanguage_(languageCode));
    },

    /**
     * Handles browse.enable_spellchecking change.
     * @param {Event} e Change event.
     * @private
     */
     updateEnableSpellCheck_: function() {
       var value = !$('enable-spell-check').checked;
       $('language-options-spell-check-language-button').disabled = value;
       if (!cr.IsMac)
         $('edit-dictionary-button').hidden = value;
     },

    /**
     * Handles spellCheckDictionaryPref change.
     * @param {Event} e Change event.
     * @private
     */
    handleSpellCheckDictionaryPrefChange_: function(e) {
      var languageCode = e.value.value;
      this.spellCheckDictionary_ = languageCode;
      var languageOptionsList = $('language-options-list');
      var selectedLanguageCode = languageOptionsList.getSelectedLanguageCode();
      if (!cr.isMac)
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
                                languageCode, true);
      chrome.send('spellCheckLanguageChange', [languageCode]);
    },

    /**
     * Checks whether it's possible to remove the language specified by
     * languageCode and returns true if possible. This function returns false
     * if the removal causes the number of preload engines to be zero.
     *
     * @param {string} languageCode Language code (ex. "fr").
     * @return {boolean} Returns true on success.
     * @private
     */
    canDeleteLanguage_: function(languageCode) {
      // First create the set of engines to be removed from input methods
      // associated with the language code.
      var enginesToBeRemovedSet = {};
      var inputMethodIds = this.languageCodeToInputMethodIdsMap_[languageCode];
      for (var i = 0; i < inputMethodIds.length; i++) {
        enginesToBeRemovedSet[inputMethodIds[i]] = true;
      }

      // Then eliminate engines that are also used for other active languages.
      // For instance, if "xkb:us::eng" is used for both English and Filipino.
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
      return (newPreloadEngines.length > 0);
    },

    /**
     * Saves the filtered extension preference.
     * @private
     */
    saveFilteredExtensionPref_: function() {
      Preferences.setStringPref(this.filteredExtensionImesPref,
                                this.filteredExtensionImes_.join(','), true);
    },

    /**
     * Updates the checkboxes in the input method list from the filtered
     * extensions preference.
     * @private
     */
    updateCheckboxesFromFilteredExtensions_: function() {
      // Convert the list into a dictonary for simpler lookup.
      var dictionary = {};
      for (var i = 0; i < this.filteredExtensionImes_.length; i++)
        dictionary[this.filteredExtensionImes_[i]] = true;

      var inputMethodList = $('language-options-input-method-list');
      var checkboxes = inputMethodList.querySelectorAll('input');
      for (var i = 0; i < checkboxes.length; i++) {
        if (checkboxes[i].inputMethodId.match(/^_ext_ime_/))
          checkboxes[i].checked = !(checkboxes[i].inputMethodId in dictionary);
      }
    },

    /**
     * Updates the filtered extensions preference from the checkboxes in the
     * input method list.
     * @private
     */
    updateFilteredExtensionsFromCheckboxes_: function() {
      this.filteredExtensionImes_ = [];
      var inputMethodList = $('language-options-input-method-list');
      var checkboxes = inputMethodList.querySelectorAll('input');
      for (var i = 0; i < checkboxes.length; i++) {
        if (checkboxes[i].inputMethodId.match(/^_ext_ime_/)) {
          if (!checkboxes[i].checked)
            this.filteredExtensionImes_.push(checkboxes[i].inputMethodId);
        }
      }
    },

    /**
     * Saves the preload engines preference.
     * @private
     */
    savePreloadEnginesPref_: function() {
      Preferences.setStringPref(this.preloadEnginesPref,
                                this.preloadEngines_.join(','), true);
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
        if (!checkboxes[i].inputMethodId.match(/^_ext_ime_/))
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
        if (!checkboxes[i].inputMethodId.match(/^_ext_ime_/)) {
          if (checkboxes[i].checked)
            this.preloadEngines_.push(checkboxes[i].inputMethodId);
        }
      }
      var languageOptionsList = $('language-options-list');
      languageOptionsList.updateDeletable();
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
      var list = loadTimeData.getValue('inputMethodList');
      for (var i = 0; i < list.length; i++) {
        dictionary[list[i].id] = true;
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

    // TODO(kochi): This is an adapted copy from new_tab.js.
    // If this will go as final UI, refactor this to share the component with
    // new new tab page.
    /**
     * Shows notification
     * @private
     */
    notificationTimeout_: null,
    showNotification_: function(text, actionText, opt_delay) {
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
    },

    onDictionaryDownloadBegin_: function(languageCode) {
      this.spellcheckDictionaryDownloadStatus_[languageCode] =
          DOWNLOAD_STATUS.IN_PROGRESS;
      if (!cr.isMac &&
          languageCode ==
              $('language-options-list').getSelectedLanguageCode()) {
        this.updateSpellCheckLanguageButton_(languageCode);
      }
    },

    onDictionaryDownloadSuccess_: function(languageCode) {
      delete this.spellcheckDictionaryDownloadStatus_[languageCode];
      this.spellcheckDictionaryDownloadFailures_ = 0;
      if (!cr.isMac &&
          languageCode ==
              $('language-options-list').getSelectedLanguageCode()) {
        this.updateSpellCheckLanguageButton_(languageCode);
      }
    },

    onDictionaryDownloadFailure_: function(languageCode) {
      this.spellcheckDictionaryDownloadStatus_[languageCode] =
          DOWNLOAD_STATUS.FAILED;
      this.spellcheckDictionaryDownloadFailures_++;
      if (!cr.isMac &&
          languageCode ==
              $('language-options-list').getSelectedLanguageCode()) {
        this.updateSpellCheckLanguageButton_(languageCode);
      }
    }
  };

  /**
   * Shows the node at |index| in |nodes|, hides all others.
   * @param {Array<HTMLElement>} nodes The nodes to be shown or hidden.
   * @param {number} index The index of |nodes| to show.
   */
  function showMutuallyExclusiveNodes(nodes, index) {
    assert(index >= 0 && index < nodes.length);
    for (var i = 0; i < nodes.length; ++i) {
      assert(nodes[i] instanceof HTMLElement);  // TODO(dbeam): Ignore null?
      nodes[i].hidden = i != index;
    }
  }

  /**
   * Chrome callback for when the UI language preference is saved.
   * @param {string} languageCode The newly selected language to use.
   */
  LanguageOptions.uiLanguageSaved = function(languageCode) {
    this.prospectiveUiLanguageCode_ = languageCode;

    // If the user is no longer on the same language code, ignore.
    if ($('language-options-list').getSelectedLanguageCode() != languageCode)
      return;

    // Special case for when a user changes to a different language, and changes
    // back to the same language without having restarted Chrome or logged
    // in/out of ChromeOS.
    if (languageCode == loadTimeData.getString('currentUiLanguageCode')) {
      LanguageOptions.getInstance().currentLocaleWasReselected();
      return;
    }

    // Otherwise, show a notification telling the user that their changes will
    // only take effect after restart.
    showMutuallyExclusiveNodes([$('language-options-ui-language-button'),
                                $('language-options-ui-notification-bar')], 1);
  };

  LanguageOptions.onDictionaryDownloadBegin = function(languageCode) {
    LanguageOptions.getInstance().onDictionaryDownloadBegin_(languageCode);
  };

  LanguageOptions.onDictionaryDownloadSuccess = function(languageCode) {
    LanguageOptions.getInstance().onDictionaryDownloadSuccess_(languageCode);
  };

  LanguageOptions.onDictionaryDownloadFailure = function(languageCode) {
    LanguageOptions.getInstance().onDictionaryDownloadFailure_(languageCode);
  };

  // Export
  return {
    LanguageOptions: LanguageOptions
  };
});
