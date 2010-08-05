// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  const OptionsPage = options.OptionsPage;
  const AddLanguageOverlay = options.language.AddLanguageOverlay;
  const LanguageList = options.language.LanguageList;

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

      this.addEventListener('visibleChange',
                            cr.bind(this.handleVisibleChange_, this));

      this.initializeInputMethodList_();

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
        label.languageCode = inputMethod.languageCode;

        inputMethodList.appendChild(label);
      }
      // Listen to pref change once the input method list is initialized.
      Preferences.getInstance().addEventListener(this.preloadEnginesPref,
          cr.bind(this.handlePreloadEnginesPrefChange_, this));
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
      } else {
        // Otherwise, users can click on the button to change the UI language.
        uiLanguageButton.textContent =
            localStrings.getString('display_in_this_language');
        uiLanguageButton.className = '';
        // Send the change request to Chrome.
        uiLanguageButton.onclick = function(e) {
          chrome.send('uiLanguageChange', [languageCode]);
        }
      }
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
        if (labels[i].languageCode == languageCode) {
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
      // Disable input methods associated with |languageCode|.
      this.removePreloadEnginesByLanguageCode_(languageCode);
      languageOptionsList.removeSelectedLanguage();
    },

    /**
     * Removes preload engines associated with the given language code.
     * @param {string} languageCode Language code (ex. "fr").
     * @private
     */
    removePreloadEnginesByLanguageCode_: function(languageCode) {
      // First create the set of engines to be removed.
      var enginesToBeRemoved = {};
      var inputMethodList = templateData.inputMethodList;
      for (var i = 0; i < inputMethodList.length; i++) {
        var inputMethod = inputMethodList[i];
        if (inputMethod.languageCode == languageCode) {
          enginesToBeRemoved[inputMethod.id] = true;
        }
      }

      // Update the preload engine list with the to-be-removed set.
      var newPreloadEngines = [];
      for (var i = 0; i < this.preloadEngines_.length; i++) {
        if (!this.preloadEngines_[i] in enginesToBeRemoved) {
          newPreloadEngines.push(this.preloadEngines_[i]);
        }
      }
      this.preloadEngines_ = newPreloadEngines;
      this.savePreloadEnginesPref_();
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
    // TODO(satorux): Show the message in a nicer way once we get a mock
    // from UX.
    alert(localStrings.getString('restart_required'));
  };

  // Export
  return {
    LanguageOptions: LanguageOptions
  };

});
