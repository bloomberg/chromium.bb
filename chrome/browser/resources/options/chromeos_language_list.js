// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.language', function() {
  const ArrayDataModel = cr.ui.ArrayDataModel;
  const LanguageOptions = options.LanguageOptions;
  const List = cr.ui.List;
  const ListItem = cr.ui.ListItem;
  const ListSingleSelectionModel = cr.ui.ListSingleSelectionModel;

  /**
   * Creates a new language list.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {cr.ui.List}
   */
  var LanguageList = cr.ui.define('list');

  /**
   * Gets display name from the given language code.
   * @param {string} languageCode Language code (ex. "fr").
   */
  LanguageList.getDisplayNameFromLanguageCode = function(languageCode) {
    // Build the language code to display name dictionary at first time.
    if (!this.languageCodeToDisplayName_) {
      this.languageCodeToDisplayName_ = {};
      var languageList = templateData.languageList;
      for (var i = 0; i < languageList.length; i++) {
        var language = languageList[i];
        this.languageCodeToDisplayName_[language.code] = language.displayName;
      }
    }

    return this.languageCodeToDisplayName_[languageCode];
  }

  LanguageList.prototype = {
    __proto__: List.prototype,

    pref: 'settings.language.preferred_languages',

    /** @inheritDoc */
    decorate: function() {
      List.prototype.decorate.call(this);
      this.selectionModel = new ListSingleSelectionModel;

      // HACK(arv): http://crbug.com/40902
      window.addEventListener('resize', cr.bind(this.redraw, this));

      // Listen to pref change.
      Preferences.getInstance().addEventListener(this.pref,
          cr.bind(this.handlePrefChange_, this));
    },

    createItem: function(languageCode) {
      var languageDisplayName =
          LanguageList.getDisplayNameFromLanguageCode(languageCode);
      return new ListItem({label: languageDisplayName});
    },

    /*
     * Adds a language to the language list.
     * @param {string} languageCode language code (ex. "fr").
     */
    addLanguage: function(languageCode) {
      this.dataModel.push(languageCode);
      // Select the last item, which is the language added.
      this.selectionModel.selectedIndex = this.dataModel.length - 1;

      this.updateBackend_();
    },

    /*
     * Gets the language codes of the currently listed languages.
     */
    getLanguageCodes: function() {
      return this.dataModel.slice();
    },

    /*
     * Gets the language code of the selected language.
     */
    getSelectedLanguageCode: function() {
      return this.selectedItem;
    },

    /*
     * Removes the currently selected language.
     */
    removeSelectedLanguage: function() {
      if (this.selectionModel.selectedIndex >= 0 &&
          // Don't allow removing the last language.
          this.dataModel.length > 1) {
        this.dataModel.splice(this.selectionModel.selectedIndex, 1);
        // Once the selected item is removed, there will be no selected item.
        // Select the item pointed by the lead index.
        this.selectionModel.selectedIndex = this.selectionModel.leadIndex;
        this.updateBackend_();
      }
    },

    /**
     * Handles pref change.
     * @param {Event} e The change event object.
     * @private
     */
    handlePrefChange_: function(e) {
      var languageCodesInCsv = e.value;
      var languageCodes = this.filterBadLanguageCodes_(
          languageCodesInCsv.split(','));
      this.load_(languageCodes);
    },

    /**
     * Loads given language list.
     * @param {Array} languageCodes List of language codes.
     * @private
     */
    load_: function(languageCodes) {
      // Preserve the original selected index. See comments below.
      var originalSelectedIndex = (this.selectionModel ?
                                   this.selectionModel.selectedIndex : -1);
      this.dataModel = new ArrayDataModel(languageCodes);
      if (originalSelectedIndex >= 0 &&
          originalSelectedIndex < this.dataModel.length) {
        // Restore the original selected index if the selected index is
        // valid after the data model is loaded. This is neeeded to keep
        // the selected language after the languge is added or removed.
        this.selectionModel.selectedIndex = originalSelectedIndex;
      } else if (this.dataModel.length > 0){
        // Otherwise, select the first item if it's not empty.
        // Note that ListSingleSelectionModel won't select an item
        // automatically, hence we manually select the first item here.
        this.selectionModel.selectedIndex = 0;
      }
    },

    /**
     * Updates backend.
     */
    updateBackend_: function() {
      // Encode the language codes into a CSV string.
      Preferences.setStringPref(this.pref, this.dataModel.slice().join(','));
    },

    /**
     * Filters bad language codes in case bad language codes are
     * stored in the preference.
     * @param {Array} languageCodes List of language codes.
     * @private
     */
    filterBadLanguageCodes_: function(languageCodes) {
      var filteredLanguageCodes = [];
      for (var i = 0; i < languageCodes.length; i++) {
        // Check if the translation for the language code is
        // present. Otherwise, skip it.
        if (LanguageList.getDisplayNameFromLanguageCode(languageCodes[i])) {
          filteredLanguageCodes.push(languageCodes[i]);
        }
      }
      return filteredLanguageCodes;
    },
  };

  return {
    LanguageList: LanguageList
  };
});
