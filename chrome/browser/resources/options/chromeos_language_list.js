// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.language', function() {
  const List = cr.ui.List;
  const ListItem = cr.ui.ListItem;
  const ArrayDataModel = cr.ui.ArrayDataModel;

  /**
   * Creates a new language list.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {cr.ui.List}
   */
  var LanguageList = cr.ui.define('list');

  LanguageList.prototype = {
    __proto__: List.prototype,

    pref: 'settings.language.preferred_languages',

    /** @inheritDoc */
    decorate: function() {
      List.prototype.decorate.call(this);

      // HACK(arv): http://crbug.com/40902
      window.addEventListener('resize', cr.bind(this.redraw, this));

      // Listen to pref change.
      Preferences.getInstance().addEventListener(this.pref,
          cr.bind(this.handlePrefChange_, this));
    },

    createItem: function(languageCode) {
      var languageDisplayName = localStrings.getString(languageCode);
      return new ListItem({label: languageDisplayName});
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
      this.dataModel = new ArrayDataModel(languageCodes);
      // Select the first item if it's not empty.
      // TODO(satorux): Switch to a single item selection model that does
      // not allow no selection, one it's ready: crbug.com/49893
      if (this.dataModel.length > 0)
        this.selectionModel.selectedIndex = 0;
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
        if (localStrings.getString(languageCodes[i])) {
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
