// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
          cr.bind(this.handleLanguageOptionsListChange_, this));
      languageOptionsList.addEventListener('save',
          cr.bind(this.handleLanguageOptionsListSave_, this));

      this.addEventListener('visibleChange',
                            cr.bind(this.handleVisibleChange_, this));

      this.initializeInputMethodList_();
      this.initializeLanguageCodeToInputMehotdIdsMap_();

      // Set up add button.
      $('language-options-add-button').onclick = function(e) {
        OptionsPage.showOverlay('addLanguageOverlay');
      };
      // Set up remove button.
      $('language-options-remove-button').addEventListener('click',
          cr.bind(this.handleRemoveButtonClick_, this));

      // Setup add language overlay page.
      OptionsPage.registerOverlay(AddLanguageOverlay.getInstance());

      // Listen to user clicks on the add language list.
      var addLanguageList = $('add-language-overlay-language-list');
      addLanguageList.addEventListener('click',
          cr.bind(this.handleAddLanguageListClick_, this));
    },

    languageListInitalized_: false,
    // The preference is a CSV string that describes preload engines
    // (i.e. active input methods).
    preloadEnginesPref: 'settings.language.preload_engines',
    preloadEngines_: [],
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
                               cr.bind(this.handleCheckboxClick_, this));
        var label = document.createElement('label');
        label.appendChild(input);
        label.appendChild(document.createTextNode(inputMethod.displayName));
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
          cr.bind(this.handlePreloadEnginesPrefChange_, this));
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
        // Cancel the event so that the click event is not propagated to
        // handleCheckboxClick_(). The button click should not be handled
        // as checkbox click.
        e.preventDefault();
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
      if (!this.languageListInitalized_ && this.visible) {
        this.languageListInitalized_ = true;
        $('language-options-list').redraw();
      }
    },

    /**
     * Handles languageOptionsList's change event.
     * @param {Event} e Change event.
     * @private
     */
    handleLanguageOptionsListChange_: function(e) {
      var languageOptionsList = $('language-options-list');
      var index = languageOptionsList.selectionModel.selectedIndex;
      if (index == -1)
        return;

      var languageCode = languageOptionsList.getLanguageCodes()[index];
      this.updateSelectedLanguageName_(languageCode);
      this.updateUiLanguageButton_(languageCode);
      this.updateInputMethodList_(languageCode);
      this.updateLanguageListInAddLanguageOverlay_();
    },

    /**
     * Handles languageOptionsList's save event.
     * @param {Event} e Save event.
     * @private
     */
    handleLanguageOptionsListSave_: function(e) {
      // Handle this event to sort the preload engines per the saved
      // languages. For instance, suppose we have two languages and
      // associated input methods:
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
      for (var i = 0; i < this.preloadEngines_.length; i++) {
        preloadEngineSet[this.preloadEngines_[i]] = true;
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

      this.preloadEngines_ = newPreloadEngines;
      this.savePreloadEnginesPref_();
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
      // TODO(kochi): Generalize this notification as a component and put it
      // in js/cr/ui/notification.js .
      uiLanguageButton.style.display = 'block';
      $('language-options-ui-notification-bar').style.display = 'none';
    },

    /**
     * Updates the input method list.
     * @param {string} languageCode Language code (ex. "fr").
     * @private
     */
    updateInputMethodList_: function(languageCode) {
      // Change the visibility of the input method list. Input methods that
      // matches |languageCode| will become visible.
      var inputMethodList = $('language-options-input-method-list');
      var labels = inputMethodList.querySelectorAll('label');
      for (var i = 0; i < labels.length; i++) {
        if (languageCode in labels[i].languageCodeSet) {
          labels[i].style.display = 'block';
        } else {
          labels[i].style.display = 'none';
        }
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
      this.preloadEngines_ = this.filterBadPreloadEngines_(e.value.split(','));
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
        // TODO(satorux): Show the message in a nicer way once we get a mock
        // from UX. crosbug.com/5547.
        alert(localStrings.getString('please_add_another_input_method'));
        checkbox.checked = true;
      }
      this.updatePreloadEnginesFromCheckboxes_();
      this.savePreloadEnginesPref_();
    },

    /**
     * Handles add language list's click event.
     * @param {Event} e Click event.
     */
    handleAddLanguageListClick_ : function(e) {
      var languageOptionsList = $('language-options-list');
      languageOptionsList.addLanguage(e.target.languageCode);
      OptionsPage.clearOverlays();
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
        // TODO(satorux): Show the message in a nicer way. See above.
        alert(localStrings.getString('this_language_is_currently_in_use'));
        return;
      }
      // Don't allow removing the language if cerntain conditions are met.
      // See removePreloadEnginesByLanguageCode_() for details.
      if (!this.removePreloadEnginesByLanguageCode_(languageCode)) {
        // TODO(satorux): Show the message in a nicer way once we get a mock
        // from UX. crosbug.com/5546.
        alert(localStrings.getString('please_add_another_language'));
        return;
      }
      // Disable input methods associated with |languageCode|.
      languageOptionsList.removeSelectedLanguage();
    },

    /**
     * Removes preload engines associated with the given language code.
     * @param {string} languageCode Language code (ex. "fr").
     * @return {boolean} Returns true on success.
     * @private
     */
    removePreloadEnginesByLanguageCode_: function(languageCode) {
      // First create the set of engines to be removed.
      var enginesToBeRemoved = {};
      var inputMethodList = templateData.inputMethodList;
      for (var i = 0; i < inputMethodList.length; i++) {
        var inputMethod = inputMethodList[i];
        if (languageCode in inputMethod.languageCodeSet) {
          enginesToBeRemoved[inputMethod.id] = true;
        }
      }

      // Update the preload engine list with the to-be-removed set.
      var newPreloadEngines = [];
      for (var i = 0; i < this.preloadEngines_.length; i++) {
        if (!(this.preloadEngines_[i] in enginesToBeRemoved)) {
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
     * stored in the preference.
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
      for (var i = 0; i < preloadEngines.length; i++) {
        // Check if the preload engine is present in the
        // dictionary. Otherwise, skip it.
        if (preloadEngines[i] in dictionary) {
            filteredPreloadEngines.push(preloadEngines[i]);
        }
      }
      return filteredPreloadEngines;
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
