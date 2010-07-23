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

      // Listens to pref change.
      Preferences.getInstance().addEventListener(this.pref,
          cr.bind(this.handlePrefChange_, this));
    },

    createItem: function(languageCode) {
      var languageDisplayName = localStrings.getString(languageCode);
      return new ListItem({label: languageDisplayName});
    },

    /**
     * Handles pref change.
     * @param {Event} event The change event object.
     * @private
     */
    handlePrefChange_: function(event) {
      this.load_(event.value);
    },

    /**
     * Loads given language list.
     * @param {string} languageCodesInCsv A CSV string of language codes.
     * @private
     */
    load_: function(languageCodesInCsv) {
      var languageCodes = languageCodesInCsv.split(',');
      this.dataModel = new ArrayDataModel(languageCodes);
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
