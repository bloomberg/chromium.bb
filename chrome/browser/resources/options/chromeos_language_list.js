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
      this.load_(e.value);
    },

    /**
     * Loads given language list.
     * @param {string} languageCodesInCsv A CSV string of language codes.
     * @private
     */
    load_: function(languageCodesInCsv) {
      var languageCodes = languageCodesInCsv.split(',');
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
  };

  return {
    LanguageList: LanguageList
  };
});
