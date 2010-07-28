// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

///////////////////////////////////////////////////////////////////////////////
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
    options.language.LanguageList.decorate(languageOptionsList);

    languageOptionsList.addEventListener('change',
        cr.bind(this.handleLanguageOptionsListChange_, this));

    this.addEventListener('visibleChange',
                          cr.bind(this.handleVisibleChange_, this));

    this.initializeInputMethodList_();
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
   * Handler for OptionsPage's visible property change event.
   * @param {Event} e Property change event.
   * @private
   */
  handleVisibleChange_ : function(e) {
    if (!this.languageListInitalized_ && this.visible) {
      this.languageListInitalized_ = true;
      $('language-options-list').redraw();
    }
  },

  /**
   * Handler for languageOptionsList's change event.
   * @param {Event} e Change event.
   * @private
   */
  handleLanguageOptionsListChange_: function(e) {
    var languageOptionsList = $('language-options-list');
    var index = languageOptionsList.selectionModel.selectedIndex;
    if (index == -1)
      return;

    var languageCode = languageOptionsList.dataModel.item(index);
    var languageDisplayName = localStrings.getString(languageCode);

    $('language-options-language-name').textContent = languageDisplayName;
    // TODO(satorux): The button text should be changed to
    // 'is_displayed_in_this_language', depending on the current UI
    // language.
    $('language-options-ui-language-button').textContent = (
        localStrings.getString('display_in_this_language'));

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
