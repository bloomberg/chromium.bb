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

      var label = document.createElement('label');
      label.appendChild(input);
      label.appendChild(document.createTextNode(inputMethod.displayName));
      label.style.display = 'none';
      label.languageCode = inputMethod.languageCode;

      inputMethodList.appendChild(label);
    }
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
  }
};
